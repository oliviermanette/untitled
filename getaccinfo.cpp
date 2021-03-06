#include "getaccinfo.h"

getAccInfo::getAccInfo(QObject *parent) : QObject(parent)
{
    setNbDonnees(0);
    gfltMinValue[0][0] = 0;
    gfltMaxValue[0][0] = 0;
    gfltMinValue[0][1] = 0;
    gfltMaxValue[0][1] = 0;

    gIntNbTotalMVT[0] = 0;
    gIntNbTotalDechets[0] = 0;
    gTblIntNbMVT[0][0] = 0;
    gIntDureeEnSecondeTransmission = 0; // Durée en seconde de chaque Transmission d'enregistrement
    gintNbTotalTransmission = 0; //En nombre de Transmission
    gIntNbSuccesTransmission = 0;
    gintCurrentWatchOffset = 0;
    gintNbTotalWatches = 0;

    gFltRythmeInstantMVT = 0;
    gFltRythmeMoyenMVT = 0;
    setDureeTransmission(DUREETransmissionDEFAUT);
    setOcraRta(OCRADEFAULTRTA);

    //mydb.setDatabaseName("/home/eldecog/untitled/db/ptms.db");
    mydb.setHostName("localhost");
    mydb.setDatabaseName("vaucheptms");
    mydb.setUserName("ptms");
    mydb.setPassword("Iluv2w0rk");
    mydb.open();
    //mydb.removeDatabase("ptms.db");

    deletePreviousSessions();
    cancelActiveSessions();
}

void getAccInfo::autoreadFile(QString qstrMontreGauche, QString qstrMontreDroit){
    int lintNb=0;
    bool lblSuccess = false;

    QString filename = "";
    QString destFile = "sauvegarde";
    gshtNbMontres = 0;

    for (int lintMontre=0;lintMontre<WATCHPERAGENT;lintMontre++)
    {
        if (lintMontre==0)
            lintNb = qstrMontreDroit.length();
        else
            lintNb = qstrMontreGauche.length();
        if (lintNb>2)
        {
            gshtNbMontres++;
            if (lintMontre==0)
                filename = "*" + qstrMontreDroit + ".dat";
            else
                filename = "*" + qstrMontreGauche + ".dat";
            filemodel = new QDir(UPLOADACCPATH,filename);
            filemodel->setFilter(QDir::Files);

            //qDebug() << filemodel->entryList().count();
            if (filemodel->entryList().count()>=MAXACCFILES)
                setNbAxes(MAXACCFILES);
            else
                setNbAxes(filemodel->entryList().count());

            setNbDonnees(0);

            for (int lintNbFile=0;lintNbFile<getNbAxes();lintNbFile++)
            {
                filename = UPLOADACCPATH;
                filename.append(filemodel->entryList().at(lintNbFile));
                //qDebug() << filename;

                QFile mFile(filename);

                if (!mFile.open(QFile::ReadOnly))
                {
                    qDebug() << "Couldn't open the file";
                    return;
                }
                QByteArray blob = mFile.readAll();
                if (MAXFILESIZE >= blob.count()/4)
                    setNbDonnees(blob.count()/4);
                else
                    setNbDonnees(MAXFILESIZE);

                lintNb = getNbDonnees();

                gfltAcc[lintNbFile][0][lintMontre] = *reinterpret_cast<float*>(blob.data());
                gfltMinValue[lintNbFile][lintMontre] = gfltAcc[lintNbFile][0][lintMontre];
                gfltMaxValue[lintNbFile][lintMontre] = gfltAcc[lintNbFile][0][lintMontre];
                for (int i=1;i<lintNb;i++)
                {
                    gfltAcc[lintNbFile][i][lintMontre] = *reinterpret_cast<float*>(blob.data()+4*i);
                    if (gfltAcc[lintNbFile][i][lintMontre]>gfltMaxValue[lintNbFile][lintMontre])
                        gfltMaxValue[lintNbFile][lintMontre] = gfltAcc[lintNbFile][i][lintMontre];
                    if (gfltAcc[lintNbFile][i][lintMontre]<gfltMinValue[lintNbFile][lintMontre])
                        gfltMinValue[lintNbFile][lintMontre] = gfltAcc[lintNbFile][i][lintMontre];
                }

                mFile.close();

                //filemodel->remove(filename);
                // Deplace le fichier dans un repertoire de sauvegarde :
                destFile = UPLOADACCPATH + SAVEFOLDER + filemodel->entryList().at(lintNbFile);
                filemodel->rename(filename,destFile);

                //qDebug() << "The number of elements read is : " << getNbDonnees();
                //qDebug() << "The '1st' value read is : " << getValueX(1);
                lblSuccess = true;
            }
            delete filemodel;
        }
    }
    if (gintNbTotalTransmission<NBMAXTransmissionS)
        gintNbTotalTransmission++; // peu importe qu'il ait réussi ou non à lire le fichier, il y a eu une nouvelle tentative de Transmission
    else
        gintNbTotalTransmission = 0;

    if (lblSuccess)
        gIntNbSuccesTransmission++;
}

int getAccInfo::readSessionFiles(){
    // Lit dans la BDD les sessions en cours :
    if (!mydb.open()){
        return 0;
    }
    else{
        //qDebug() << "Database is opened!";
        QSqlQuery sqry(mydb);
        //QString lstQuery = "SELECT distinct codeID, date_debut, sessions.Id from identites, sessions, montres WHERE sessions.Actif=2 AND sessions.Identite=identites.Id AND ((montres.Id = Montre_Gauche) OR (montres.Id=  Montre_Droit))";

        // Nouvelle requête :::
        // SELECT codeID, date_debut, sessions.Id, Montre_Gauche, montres.Id from identites JOIN sessions ON sessions.Identite=identites.Id JOIN montres ON ((montres.Id = Montre_Gauche) OR (montres.Id=  Montre_Droit)) WHERE sessions.Actif=2
        // :::::::::::::::::::::::::::
        QString lstQuery = "SELECT codeID, date_debut, sessions.Id, Montre_Gauche, montres.Id from identites JOIN sessions ON sessions.Identite=identites.Id JOIN montres ON ((montres.Id = Montre_Gauche) OR (montres.Id=  Montre_Droit)) WHERE sessions.Actif=2";
        //qDebug() << lstQuery;
        if (sqry.exec(lstQuery)){
            short lintCount=0;
            short lshtAcc = 0;
            while (sqry.next()){ //sqry.first();
                //qDebug() << "1 session en cours de traitement pour enregistrement BDD : #" +lintCount;
                // Recupere les donnees a utiliser tout le temps
                QString strSerialNo = sqry.value(0).toString();
                QString strSessionId = sqry.value(2).toString();
                QString lstrFilename = "";
                int intSessionId = sqry.value(2).toInt();
                // Maintenant il faut lire le fichier correspondant a chaque ligne dont on a le serialno et le timestamp de debut en s'inspirant de autoreadfile
                QString destFile = sqry.value(1).toString();//.truncate(3); // 3 premiers caracteres du timestamp de la BDD Session
                destFile.truncate(3);
                QString filename = destFile + "*_" + strSerialNo + ".dat"; //filtre les fichiers
                //qDebug() << "Filtre utilise : "+ filename;
                unsigned int lintNb=0;
                unsigned long luintTimestamp = 0;
                unsigned long luintStartingFileOffset=0;
                unsigned long luintTimeOffset = 0;
                bool lblCheckTS = true;

                filemodel = new QDir(UPLOADACCPATH,filename);
                filemodel->setFilter(QDir::Files);

                // verifie le timestamp
                //qDebug() << "We found file fiting this filter : "+ QString::number(filemodel->entryList().count());
                int lintI = 0;
                while (lblCheckTS && (filemodel->entryList().count()>lintI)){
                    //qDebug() << "We found file fiting this filter : "+ QString::number(filemodel->entryList().count());
                    //qDebug() << "Timestamp de la BDD : " + QString::number(sqry.value(1).toULongLong());
                    filename = filemodel->entryList().at(lintI);
                    destFile = filename.split("_")[0];
                    luintTimestamp = destFile.toULongLong(); // contient le timestamp du fichier, on doit verifier que c'est superieur au timestamp de la BDD Session.date_debut
                    //qDebug() << "Timestamp du fichier a verifier (AVANT conversion): "+ destFile;
                    //qDebug() << "Timestamp du fichier a verifier : "+ QString::number(luintTimestamp);
                    if (luintTimestamp >= sqry.value(1).toULongLong()){
                        //c'est bon
                        luintStartingFileOffset = lintI;
                        lblCheckTS = false;
                        luintTimeOffset = luintTimestamp - sqry.value(1).toULongLong();
                        //qDebug() << "Timestamp superieur fichiers trouves";
                    }
                    else{
                        // ca ne va pas il faut regarder le fichier suivant
                        //qDebug() << "AUCUN fichiers trouves inside";
                        lintI++;
                    }
                }
                //S'il a regard2 tous les fichiers sans trouver :> erreur
                if (lblCheckTS){
                    //qDebug() << "AUCUN fichiers trouves";
                    return 0;
                }

                if (filemodel->entryList().count()>=MAXACCFILES)
                    setNbAxes(MAXACCFILES);
                else
                    setNbAxes(filemodel->entryList().count());
                lshtAcc = 0;

                for (unsigned int lintNbFile=luintStartingFileOffset;lintNbFile<luintStartingFileOffset+getNbAxes();lintNbFile++){
                    filename = UPLOADACCPATH;
                    lstrFilename = filemodel->entryList().at(lintNbFile);
                    filename.append(lstrFilename);
                    // qDebug() << " Ouverture du fichier : " + filemodel->entryList().at(lintNbFile);

                    QFile mFile(filename);

                    if (!mFile.open(QFile::ReadOnly)){
                        //qDebug() << "Couldn't open the file";
                        return lintCount;
                    }
                    QByteArray blob =mFile.readAll();
                    if (MAXFILESIZE>=blob.count()/4)
                        lintNb =  blob.count()/4;
                    else
                        lintNb = MAXFILESIZE;

                    setNbDonnees(lintNb);

                    //qDebug() << "Nombre d'elements dans le vecteur : "+ QString::number(lintNb);
                    gfltAcc[lshtAcc][0][0] = *reinterpret_cast<float*>(blob.data());
                    gfltMinValue[lshtAcc][0] = gfltAcc[lshtAcc][0][0];
                    gfltMaxValue[lshtAcc][0] = gfltAcc[lshtAcc][0][0];
                    for (unsigned int i=1;i<lintNb;i++){
                        gfltAcc[lshtAcc][i][0] = *reinterpret_cast<float*>(blob.data()+4*i);
                        if (gfltAcc[lshtAcc][i][0]>gfltMaxValue[lshtAcc][0])
                            gfltMaxValue[lshtAcc][0] = gfltAcc[lshtAcc][i][0];
                        if (gfltAcc[lshtAcc][i][0]<gfltMinValue[lshtAcc][0])
                            gfltMinValue[lshtAcc][0] = gfltAcc[lshtAcc][i][0];
                    }
                    //qDebug() << "The file is opened and read !";

                    mFile.close();

                    //qDebug() << "// Deplace le fichier dans un repertoire de sauvegarde :";
                    destFile = UPLOADACCPATH + SAVEFOLDER + filemodel->entryList().at(lintNbFile);
                    filemodel->rename(filename,destFile);
                    lshtAcc++;
                }

                delete filemodel;

                //qDebug() << "// Il faut maintenant calculer le nb d actions";
                int lintNbMvt = 0, lintNbDechet = 0;

                float lfltCoeff[MAXACCFILES], lfltMaxCoef=0;
                float lfltTotal = 0;
                for (int lintJ=0;lintJ<getNbAxes();lintJ++){
                    lfltCoeff[lintJ] = qAbs(getAmplitude(lintJ) * qAbs(getAmplitude(lintJ)*getMean(lintJ))*sum(&gfltAcc[lintJ][0][0],lintNb,0,lintNb));
                    //qDebug() << "getAmplitude : " + QString::number(qAbs(getAmplitude(lintJ)));
                    //qDebug() << "ABS : " + QString::number(qAbs(getAmplitude(lintJ));
                    //qDebug() << "getMean : " + QString::number(getMean(lintJ));
                    //qDebug() << "sum : " + QString::number(sum(&gfltAcc[lintJ][0][0],lintNb,0,lintNb));

                    lfltTotal += lfltCoeff[lintJ];
                    if (lfltCoeff[lintJ]>lfltMaxCoef)
                        lfltMaxCoef = lfltCoeff[lintJ];
                    //qDebug() << "coef("<<lintJ<<") : "<< lfltCoeff[lintJ];
                }
                for (int lintJ=0;lintJ<getNbAxes();lintJ++)
                    lfltCoeff[lintJ] /= lfltMaxCoef;
                if (lfltTotal<(1000000))
                    return 0;

                for (unsigned int lintI=0;lintI<lintNb;lintI++){
                    gfltCombinaisonAcc[0][lintI] = 0;
                    for (int lintJ=0;lintJ<getNbAxes();lintJ++)
                        gfltCombinaisonAcc[0][lintI] += lfltCoeff[lintJ]*getValue(lintI,lintJ,0);
                }
                gfltWindowAverage[TAILLEOFF-1]=0;
                for (int i=TAILLEOFF;i<lintNb;i++){
                    gbolChangeDirection[i] = 0;
                    gfltWindowAverage[i] = mean(&gfltCombinaisonAcc[0][0],lintNb,(i-TAILLEWIN+1),i);
                    //qDebug() << "ValeurDetection : " << (gfltWindowAverage[i])<<"<- 4: "<< gfltCombinaisonAcc[i-3]<<"; 3: "<< gfltCombinaisonAcc[i-2]<<"; 2: "<< gfltCombinaisonAcc[i-1]<<"; 1: "<< gfltCombinaisonAcc[i];
                    gfltWindowAverage[i] = gfltWindowAverage[i] - mean(&gfltCombinaisonAcc[0][0],lintNb,reinterpret_cast<int>(i-TAILLEOFF+1),reinterpret_cast<int>(i));

                    if ((gfltWindowAverage[i-1]/gfltWindowAverage[i])<SEUILDETECTIONDechets)
                        if (qAbs(gfltWindowAverage[i]-gfltWindowAverage[i-TAILLEWIN+1])>SEUILACCEG)
                            lintNbDechet++;

                    if ((gfltWindowAverage[i-1]/gfltWindowAverage[i])<SEUILDETECTION)
                        if (qAbs(gfltWindowAverage[i]-gfltWindowAverage[i-TAILLEWIN+1])>SEUILACCEG){
                            gbolChangeDirection[i] = 1;
                            lintNbMvt++;
                        }
                }

                lintNbMvt /=2;
                lintNbDechet/=2;
                //qDebug() << "Le nombre de mouvements : " + QString::number(lintNbMvt);
                qDebug() << "Le nombre de dechets : " + QString::number(lintNbDechet);

              //  strSessionId+ ", " + strSerialNo + ", " + QString::number(luintTimeOffset) + ", " + QString::number(getDureeTransmission()) + ", " + QString::number(lintNbMvt) + ", " + QString::number(lintNbDechet) + ")");

                // indice OCRA
                    // calcul de l'indice ATA (actual Technical Actions : represente le nombre d'actions techniques que l'on effectue pendant toute la duree du poste.
                    // Calcul du RTA : k  *  F * P * R * Fa * Rc * D
                        // k = 30
                        // F = 0,7
                        // P = 0,8 facteur de posture (etendu des mouvements)
                        // R = 0.9
                        // Fa = 0,8
                        // f : somme de la freq des mouvements par minute de chaque bras
                    // calcul final : ATA/10
                float lfltOCRAindex =  getOCRA4DBSession(lintNbMvt);//lintNbMvt*6.0/12.0;
                //qDebug() << lfltOCRAindex;
                /*
                 * Risque OCRA
                    Conclusions
                    Vert    :   < 2,2       Pas de risque
                    Jaune   :   2,3 - 3,5   Risque faible, moins du double que pour la case verte
                    Rouge   :   > 3,5       Risque plus de deux fois plus grand que pour la case verte
                Sur base d'études récentes, la relation entre l'indice OCRA et la prévalence de TMS
                a été estimée par la formule de régression suivante:
                Prévalence = 2,39  x OCRA, l'erreur standard sur le coefficient 2,39 est égal à ± 0,14
                Les valeurs limites des zones de risque correspondent à 95% de la population non
                exposée et donne la zone verte. Pour la zone rouge, les limites ont été choisies de
                telle sorte que 50% des personnes présentent le double de plaintes que la populati-
                on non exposée */

                // niveau de risque : comprend en plus d'autres parametres
                float lfltRisque = getDbNiveauDeRisque(intSessionId);

                // nb_zone_rouge
                // nb_zone_verte
                // nb_zone_jaune

                // vibrations
                // nb charges lourdes

                //et tout mettre dans la table enregistrements
                QSqlQuery nquery(mydb);
                QString lstQuery = "INSERT INTO enregistrements (Session, serialno_montre, time_offset, duree, nb_actions, nb_objets, indice_OCRA, niveau_risque, filename";
                        if (sqry.value(3)==sqry.value(4)) lstQuery +=", poignet_gauche";
                        lstQuery += ") VALUES (" +
                        strSessionId+ ", '" +
                        strSerialNo + "', " +
                        QString::number(luintTimeOffset) + ", " +
                        QString::number(getDureeTransmission()) + ", " +
                        QString::number(lintNbMvt) + ", " +
                        QString::number(lintNbDechet)+ ", " +
                        QString::number(lfltOCRAindex, 'f', 2)+ ", " +
                        QString::number(lfltRisque, 'f', 2)+ ", '" +
                        lstrFilename + "'";
                        if (sqry.value(3)==sqry.value(4)) lstQuery +=", 1";
                        lstQuery += ")";

                //qDebug() << lstQuery;
                nquery.exec(lstQuery);
                //qDebug() << //retourner le nombre de fichiers lus avec succes;
                lintCount++;
                //qDebug() << sqry.value(0).toString();
            }
            return lintCount;
        }
        else{
            qDebug() << "mmh something got wrong while retrieving the data";
            qDebug() << "check : "<< lstQuery;
            return 0;
        }
    }
}

int getAccInfo::addSessionFiles(QString lstr1TimeStamp, int lintIndividu, int lintSessiondurationInSec){
    QString filename = "*_*_*.dat"; //filtre les fichiers
    QString strSessionId;
    QString strSerialNo;
    setNbAxes(MAXACCFILES);
    int lintCount=0;
    bool lblFirstFileofSession = true;
    filemodel = new QDir(UPLOADACCPATH,filename);
    filemodel->setFilter(QDir::Files);
    for (int lintNbFile=0;lintNbFile<filemodel->entryList().count();lintNbFile++){
        filename = filemodel->entryList().at(lintNbFile);
        filename = filename.split("_")[2].split(".")[0];
        long long lUintCurrentTimeStamp =filename.toLongLong();
        long long lulngTimeStamp = lstr1TimeStamp.toLongLong();
        unsigned int lintNb=0;
        if (lUintCurrentTimeStamp>=lulngTimeStamp){
            // seulement si c'est dans la meme session
            if (lUintCurrentTimeStamp<lulngTimeStamp+90000+lintSessiondurationInSec*1000){
                if (lblFirstFileofSession){
                    QString lstQuery = "INSERT INTO sessions (date_debut,Identite,Duree,Actif) VALUES ('"+
                            filename+"', '"+QString::number(lintIndividu)+"', '"+QString::number(lintSessiondurationInSec*1000)+"', '0')"; // Ferme la session
                    //qDebug() << lstQuery;
                    QSqlQuery nquery(mydb);
                    nquery.exec(lstQuery);
                    //recupere l'Id de session
                    lstQuery = "SELECT Id FROM sessions WHERE date_debut='"+ filename+"'";
                    if (nquery.exec(lstQuery)){
                        if (nquery.first()){
                            strSessionId = nquery.value(0).toString();
                        }
                    }
                    // recupere le serial No
                    filename = filemodel->entryList().at(lintNbFile);
                    strSerialNo = filename.split("_")[0];
                    lblFirstFileofSession = false;
                }
                // Ajoute les infos a l'enregistrement
                lintCount++;
                int luintStartingFileOffset = lintNbFile;
                int lchrAcc = 0;
                for (int luintI=luintStartingFileOffset;luintI<luintStartingFileOffset+getNbAxes();luintI++){
                    filename = UPLOADACCPATH;
                    filename.append(filemodel->entryList().at(luintI));

                    QFile mFile(filename);

                    if (!mFile.open(QFile::ReadOnly)){
                        qDebug() << "Couldn't open the file";
                        return lintCount;
                    }
                    QByteArray blob =mFile.readAll();
                    if (MAXFILESIZE>=blob.count()/4)
                        lintNb =  blob.count()/4;
                    else
                        lintNb = MAXFILESIZE;

                    setNbDonnees(lintNb);

                    gfltAcc[lchrAcc][0][0] = *reinterpret_cast<float*>(blob.data());
                    gfltMinValue[lchrAcc][0] = gfltAcc[lchrAcc][0][0];
                    gfltMaxValue[lchrAcc][0] = gfltAcc[lchrAcc][0][0];
                    for (unsigned int i=1;i<lintNb;i++){
                        gfltAcc[lchrAcc][i][0] = *reinterpret_cast<float*>(blob.data()+4*i);
                        if (gfltAcc[lchrAcc][i][0]>gfltMaxValue[lchrAcc][0])
                            gfltMaxValue[lchrAcc][0] = gfltAcc[lchrAcc][i][0];
                        if (gfltAcc[lchrAcc][i][0]<gfltMinValue[lchrAcc][0])
                            gfltMinValue[lchrAcc][0] = gfltAcc[lchrAcc][i][0];
                    }
                    //qDebug() << "The file is opened and read !";

                    mFile.close();

                    //qDebug() << "// Deplace le fichier dans un repertoire de sauvegarde :";
                    QString destFile = UPLOADACCPATH + SAVEFOLDER + filemodel->entryList().at(luintI);
                    filemodel->rename(filename,destFile);
                    lchrAcc++;
                    lintNbFile = luintI;
                }
                //qDebug() << "// Il faut maintenant calculer le nb d'actions";
                int lintNbMvt = 0, lintNbDechet = 0;

                float lfltCoeff[MAXACCFILES], lfltMaxCoef=0;
                float lfltTotal = 0;
                for (int lintJ=0;lintJ<getNbAxes();lintJ++){
                    lfltCoeff[lintJ] = qAbs(getAmplitude(lintJ) * qAbs(getAmplitude(lintJ)*getMean(lintJ))*sum(&gfltAcc[lintJ][0][0],lintNb,0,lintNb));
                    //qDebug() << "getAmplitude : " + QString::number(qAbs(getAmplitude(lintJ)));
                    //qDebug() << "ABS : " + QString::number(qAbs(getAmplitude(lintJ)));
                    //qDebug() << "getMean : " + QString::number(getMean(lintJ));
                    //qDebug() << "sum : " + QString::number(sum(&gfltAcc[lintJ][0][0],lintNb,0,lintNb));

                    lfltTotal += lfltCoeff[lintJ];
                    if (lfltCoeff[lintJ]>lfltMaxCoef)
                        lfltMaxCoef = lfltCoeff[lintJ];
                    //qDebug() << "coef("<<lintJ<<") : "<< lfltCoeff[lintJ];
                }
                for (int lintJ=0;lintJ<getNbAxes();lintJ++)
                    lfltCoeff[lintJ] /= lfltMaxCoef;
                if (lfltTotal<(1000000))
                    return 0;

                for (unsigned int lintI=0;lintI<lintNb;lintI++){
                    gfltCombinaisonAcc[0][lintI] = 0;
                    for (int lintJ=0;lintJ<getNbAxes();lintJ++)
                        gfltCombinaisonAcc[0][lintI] += lfltCoeff[lintJ]*getValue(lintI,lintJ,0);
                }
                gfltWindowAverage[TAILLEOFF-1]=0;
                for (int i=TAILLEOFF;i<lintNb;i++){
                    gbolChangeDirection[i] = 0;
                    gfltWindowAverage[i] = mean(&gfltCombinaisonAcc[0][0],lintNb,(i-TAILLEWIN+1),i);
                    //qDebug() << "ValeurDetection : " << (gfltWindowAverage[i])<<"<- 4: "<< gfltCombinaisonAcc[i-3]<<"; 3: "<< gfltCombinaisonAcc[i-2]<<"; 2: "<< gfltCombinaisonAcc[i-1]<<"; 1: "<< gfltCombinaisonAcc[i];
                    gfltWindowAverage[i] = gfltWindowAverage[i] - mean(&gfltCombinaisonAcc[0][0],lintNb,reinterpret_cast<int>(i-TAILLEOFF+1),reinterpret_cast<int>(i));

                    if ((gfltWindowAverage[i-1]/gfltWindowAverage[i])<SEUILDETECTIONDechets)
                        if (qAbs(gfltWindowAverage[i]-gfltWindowAverage[i-TAILLEWIN+1])>SEUILACCEG)
                            lintNbDechet++;

                    if ((gfltWindowAverage[i-1]/gfltWindowAverage[i])<SEUILDETECTION)
                        if (qAbs(gfltWindowAverage[i]-gfltWindowAverage[i-TAILLEWIN+1])>SEUILACCEG){
                            gbolChangeDirection[i] = 1;
                            lintNbMvt++;
                        }
                }
                lintNbMvt /=2;
                lintNbDechet/=2;
                //qDebug() << "Le nombre de mouvements : " + QString::number(lintNbMvt);
                lulngTimeStamp = (lUintCurrentTimeStamp-(lulngTimeStamp));
                QString lstQuery = "INSERT INTO enregistrements (enregistrements.Session,serialno_montre,time_offset,duree,nb_actions,nb_objets) VALUES ('"+
                        strSessionId+"', '"+strSerialNo+"', '"+ QString::number(lulngTimeStamp)+"', '"+QString::number(getDureeTransmission())+"', '"+QString::number(lintNbMvt)+"', '"+QString::number(lintNbDechet)+"')";
                //qDebug() << lstQuery;
                QSqlQuery nquery(mydb);
                nquery.exec(lstQuery);
            }
        }
    }
    delete filemodel;
    return lintCount;
}

int getAccInfo::readAllSessionFiles(){
    /*
    liste des fichiers
    ordre alphabétique
    extraction du timestamp
    -- si c'est le premier stocke dans var et va au suivant
    -- si écart < 90 secondes alors considère même session
    -- si écart > alors ferme session et crée la suivante
    typedef struct FileSessions{ // https://trello.com/c/QqAHcARc
        //FileSessions() {}
        unsigned char uchrId; // nombre max de session 256
        unsigned long ulngTimeStamp; // c'est un timestamp
        unsigned short ushrtDuration;// a priori au max 2560 secondes donc on passe a short pour 65 536
        unsigned char uchrFileNumber; //nombre de fichier max 256
        unsigned int uintIDMontre;
    } FileSessions;
    FileSessions tblFileSessions[50];
    */
    //tblFileSessions[0].
    QString filename = "";
    bool lblCurrentSession = false;
    unsigned short lshtNbFileSessions =0;
    filemodel = new QDir(UPLOADACCPATH,"*_0_*.dat");
    filemodel->setFilter(QDir::Files);
    for (int lintNbFile=0;lintNbFile<filemodel->entryList().count();lintNbFile++){
        filename = filemodel->entryList().at(lintNbFile);
        //qDebug() << filename;
        if (!lblCurrentSession){
            lblCurrentSession = true;
            tblFileSessions[lshtNbFileSessions].uchrFileNumber=1;
            tblFileSessions[lshtNbFileSessions].ushrtDuration = gIntDureeEnSecondeTransmission;
            filename = filename.split('_')[2];
            filename = filename.split('.')[0];
            tblFileSessions[lshtNbFileSessions].ulngTimeStamp = filename.toLongLong();
            //qDebug() << tblFileSessions[lshtNbFileSessions].ulngTimeStamp;
            filename = filemodel->entryList().at(lintNbFile);
            tblFileSessions[lshtNbFileSessions].strIDMontre = filename.split('_')[0];
            //qDebug() << tblFileSessions[lshtNbFileSessions].strIDMontre;
        }
        else{
            //qDebug() << filename.split('_')[2];
            if (tblFileSessions[lshtNbFileSessions].strIDMontre==filename.split('_')[0]){
                filename = filename.split('_')[2];
                filename = filename.split('.')[0];
                //qDebug() << tblFileSessions[lshtNbFileSessions].uchrFileNumber;
                if (filename.toLongLong()<tblFileSessions[lshtNbFileSessions].ulngTimeStamp+90000+(tblFileSessions[lshtNbFileSessions].ushrtDuration)*1000){
                    //qDebug() << "ajoute a la session";
                    tblFileSessions[lshtNbFileSessions].uchrFileNumber++;
                    tblFileSessions[lshtNbFileSessions].ushrtDuration += gIntDureeEnSecondeTransmission;
                }
                else{
                    //qDebug() << "nouvelle session";
                    if (lshtNbFileSessions+1==MAXFILESESSIONS)
                        return lshtNbFileSessions;
                    lshtNbFileSessions++;
                    //qDebug() <<lshtNbFileSessions;
                    tblFileSessions[lshtNbFileSessions].uchrFileNumber=1;
                    tblFileSessions[lshtNbFileSessions].ushrtDuration = gIntDureeEnSecondeTransmission;
                    filename = filemodel->entryList().at(lintNbFile);
                    filename = filename.split('_')[2];
                    filename = filename.split('.')[0];
                    tblFileSessions[lshtNbFileSessions].ulngTimeStamp = filename.toLongLong();
                    filename = filemodel->entryList().at(lintNbFile);
                    //qDebug() <<filename;
                    tblFileSessions[lshtNbFileSessions].strIDMontre = filename.split('_')[0];
                    //qDebug() << tblFileSessions[lshtNbFileSessions].strIDMontre;
                }
            }
        }
    }
    delete filemodel;
    if (lblCurrentSession)
        lshtNbFileSessions++;
    return lshtNbFileSessions;
}

float getAccInfo::getValueX(int lintIndex, bool lblMontre)
{
    //qDebug() << "The requested value is : " << gfltAccX[lintIndex] << " ["<<lintIndex<<"]";
    return gfltAcc[0][lintIndex][lblMontre];
}

float getAccInfo::getValueY(int lintIndex, bool lblMontre)
{
    return gfltAcc[1][lintIndex][lblMontre];
}

float getAccInfo::getValueZ(int lintIndex, bool lblMontre)
{
    return gfltAcc[2][lintIndex][lblMontre];
}

float getAccInfo::getValue(int lintIndex, int lintAccelerometre, bool lblMontre)
{
    return gfltAcc[lintAccelerometre][lintIndex][lblMontre];
}

float getAccInfo::getCombinaison(int lintIndex, bool lblMontre)
{
     return gfltCombinaisonAcc[lblMontre][lintIndex];
}

int getAccInfo::getNbDonnees()
{
    return gintNombreDonnees;
}

int getAccInfo::getNbAxes(bool lblMontre)
{
    return gintNombreAxes[lblMontre];
}

int getAccInfo::getNbMouvements(bool lblMontre)
{
    int lintNbMvt = 0, lintNbDechet = 0;
    gIntNbMVT[lblMontre] = lintNbMvt;
    gIntNbDechets[lblMontre] = lintNbDechet; // au cas ou on ne trouve rien il dira que c'est zero

    float lfltCoeff[MAXACCFILES], lfltMaxCoef=0;
    float lfltTotal = 0;
    for (int lintJ=0;lintJ<getNbAxes();lintJ++)
    {
        lfltCoeff[lintJ] = qAbs(getAmplitude(lintJ) * qAbs(getAmplitude(lintJ)*getMean(lintJ))*sum(&gfltAcc[lintJ][0][lblMontre],getNbDonnees(),0,getNbDonnees()));
        lfltTotal += lfltCoeff[lintJ];
        if (lfltCoeff[lintJ]>lfltMaxCoef)
            lfltMaxCoef = lfltCoeff[lintJ];
        //qDebug() << "coef("<<lintJ<<") : "<< lfltCoeff[lintJ];
    }
    //qDebug() << "Total = "<< lfltMaxCoef;
    //qDebug() << "Max = "<< lfltTotal;
    for (int lintJ=0;lintJ<getNbAxes();lintJ++)
        lfltCoeff[lintJ] /= lfltMaxCoef;
    if (lfltTotal<(1000000))
        return 0;

    for (int lintI=0;lintI<getNbDonnees();lintI++)
    {
        gfltCombinaisonAcc[lblMontre][lintI] = 0;
        for (int lintJ=0;lintJ<getNbAxes();lintJ++)
            gfltCombinaisonAcc[lblMontre][lintI] += lfltCoeff[lintJ]*getValue(lintI,lintJ,lblMontre);

    }
    gfltWindowAverage[TAILLEOFF-1]=0;
    for (int i=TAILLEOFF;i<getNbDonnees();i++)
    {
        gbolChangeDirection[i] = 0;
        gfltWindowAverage[i] = mean(&gfltCombinaisonAcc[lblMontre][0],getNbDonnees(),(i-TAILLEWIN+1),i);
        //qDebug() << "ValeurDetection : " << (gfltWindowAverage[i])<<"<- 4: "<< gfltCombinaisonAcc[i-3]<<"; 3: "<< gfltCombinaisonAcc[i-2]<<"; 2: "<< gfltCombinaisonAcc[i-1]<<"; 1: "<< gfltCombinaisonAcc[i];
        gfltWindowAverage[i] = gfltWindowAverage[i] - mean(&gfltCombinaisonAcc[lblMontre][0],getNbDonnees(),reinterpret_cast<int>(i-TAILLEOFF+1),reinterpret_cast<int>(i));

        if ((gfltWindowAverage[i-1]/gfltWindowAverage[i])<SEUILDETECTIONDechets)
            if (qAbs(gfltWindowAverage[i]-gfltWindowAverage[i-TAILLEWIN+1])>SEUILACCEG)
                lintNbDechet++;

        if ((gfltWindowAverage[i-1]/gfltWindowAverage[i])<SEUILDETECTION)
            if (qAbs(gfltWindowAverage[i]-gfltWindowAverage[i-TAILLEWIN+1])>SEUILACCEG)
            {
                gbolChangeDirection[i] = 1;
                lintNbMvt++;
            }
    }

    lintNbMvt /=2;
    lintNbDechet/=2;
    gIntNbMVT[lblMontre] = lintNbMvt;
    gIntNbDechets[lblMontre] = lintNbDechet;
    gIntNbTotalMVT[lblMontre] += gIntNbMVT[lblMontre]; // Ajoute au nombre total de mouvements
    gIntNbTotalDechets[lblMontre] += lintNbDechet;
    gTblIntNbMVT[gintNbTotalTransmission][lblMontre] = gIntNbMVT[lblMontre];

    //qDebug() << "Nombre de mouvements : " << gIntNbMVT[lblMontre] <<" ("<<lblMontre << ")";

    CaracteriseMVT();

    return gIntNbMVT[lblMontre];//*/
    return lblMontre;
}

float getAccInfo::getNiveauRisque(int lintIndividu)
{
    //niveau de risaue
    //duree de travail :
    //la duree chaude 2h
    int lintDuree = getTempsTravail();// TODO cette fonction doit dependre de l individu concern2
    float lfltRisqueTampon = 0;
    lfltRisqueTampon = (lintDuree/60)/(120); //TODO au bout de combien de temps ca commence a etre chaud?

    //age
    int lintAge  = getIndividuAge(lintIndividu);
    lfltRisqueTampon += (lintAge-16)/(60-16); //TODO Age maxi et age mini correspondant au risque

    // nombre de mouvement total (AT)
    int lintNbTotalMVT = getIntNbTotalMVT();
    lintNbTotalMVT += getIntNbTotalMVT(1);// TODO cette fonction doit dependre de l individu concern2

    lfltRisqueTampon +=lintNbTotalMVT/1000; // au dela de combien d'actions technique c est chaud ?

    //le rythme moyen
    float lfltRythme = getFltRythmeMoyenMVT();// TODO cette fonction doit dependre de l individu concern2
    lfltRisqueTampon += lfltRythme/60;

    lfltRythme = getFltRythmeMoyenMVT(1);// TODO cette fonction doit dependre de l individu concern2
    lfltRisqueTampon += lfltRythme/60;

    // le rythme cardiaque
    // TODO

    // le genre
    // TODO

    // le niveau des vibrations
    // TODO

    // le nombre d'amplitudes rouges et jaune
    // TODO

    // le nombre de charges lourdes
    // TODO

    lfltRisqueTampon =  100*lfltRisqueTampon/5 ;
    return lfltRisqueTampon;
}

float getAccInfo::getMin(int lintAccelerometre, bool lblMontre)
{
    return gfltMinValue[lintAccelerometre][lblMontre];
}

float getAccInfo::getMax(int lintAccelerometre, bool lblMontre)
{
    return gfltMaxValue[lintAccelerometre][lblMontre];
}

float getAccInfo::getGeneralMax(bool lblMontre)
{
    float lfltMax =0;
    for (int lintI=0, lintNbAxes = getNbAxes();lintI<lintNbAxes;lintI++)
    {
        if (lfltMax<getMax(lintI,lblMontre))
            lfltMax = getMax(lintI,lblMontre);
    }
    return lfltMax;
}

float getAccInfo::getGeneralMin(bool lblMontre)
{
    float lfltMin =0;
    for (int lintI=0, lintNbAxes = getNbAxes();lintI<lintNbAxes;lintI++)
    {
        if (lfltMin>getMin(lintI,lblMontre))
            lfltMin = getMin(lintI,lblMontre);
    }
    return lfltMin;
}

void getAccInfo::setNbDonnees(int lintValue)
{
    gintNombreDonnees =lintValue;
}

void getAccInfo::setNbAxes(int lintAxes, bool lblMontre)
{
    gintNombreAxes[lblMontre] = lintAxes;
}

float getAccInfo::getAmplitude(int lintAccelerometre=0)
{
    return getMax(lintAccelerometre)-getMin(lintAccelerometre);
}

float getAccInfo::getMean(int lintAccelerometre)
{
    float lfltMean = getValue(0,lintAccelerometre);
    lfltMean = lfltMean/getNbDonnees();
    for (int lintI=1;lintI<getNbDonnees();lintI++)
        lfltMean += getValue(lintI,lintAccelerometre)/getNbDonnees();
    return lfltMean;
}

float getAccInfo::mean(float* lfltTableau, int lintTailleTableau, int lintDebut, int lintFin)
{
    if ((lintFin>lintDebut)&(lintDebut>0))
    {
        if (lintTailleTableau>0)
        {
            int lintNbDonnees = lintFin-lintDebut;
            float lfltMean = lfltTableau[lintDebut]/lintNbDonnees;
            for (int i=lintDebut+1;i<lintFin;i++)
                lfltMean += lfltTableau[i]/lintNbDonnees;

            return lfltMean;
        }
        else
            return -9999;
    }
    else
        return -9999;
}

float getAccInfo::sum(float *lfltTableau, int lintTailleTableau, int lintDebut, int lintFin)
{
    if ((lintFin>lintDebut)&(lintDebut>0))
    {
        if (lintTailleTableau>0)
        {
            float lfltSum = lfltTableau[lintDebut];
            for (int i=lintDebut+1;i<lintFin;i++)
                lfltSum += lfltTableau[i];

            return lfltSum;
        }
        else
            return -9999;
    }
    else
        return -9999;
}

float getAccInfo::min(float *lfltTableau, int lintTailleTableau, int lintDebut, int lintFin)
{
    float lfltSortie = -9999;
    if ((lintFin>lintDebut) & ((lintFin-lintDebut)<=lintTailleTableau))
    {
        if (lintTailleTableau>0)
        {
            lfltSortie = lfltTableau[lintDebut];
            for (int i=lintDebut+1;i<lintFin;i++)
                if (lfltSortie>lfltTableau[i])
                    lfltSortie = lfltTableau[i];
        }
        else
            return -9999;
    }
    else
        return -9999;
    return lfltSortie;
}

float getAccInfo::max(float *lfltTableau, int lintTailleTableau, int lintDebut, int lintFin)
{
    float lfltSortie = -9999;
    if ((lintFin>lintDebut) & ((lintFin-lintDebut)<=lintTailleTableau))
    {
        if (lintTailleTableau>0)
        {
            lfltSortie = lfltTableau[lintDebut];
            for (int i=lintDebut+1;i<lintFin;i++)
                if (lfltSortie<lfltTableau[i])
                    lfltSortie = lfltTableau[i];
        }
        else
            return -9999;
    }
    else
        return -9999;
    return lfltSortie;
}

int getAccInfo::maxPos(float *lfltTableau, int lintTailleTableau, int lintDebut, int lintFin)
{
    float lfltSortie = -9999;
    int lintSortie = -9999;
    if ((lintFin>lintDebut) & ((lintFin-lintDebut)<=lintTailleTableau))
    {
        if (lintTailleTableau>0)
        {
            lfltSortie = lfltTableau[lintDebut];
            lintSortie = lintDebut;
            for (int i=lintDebut+1;i<lintFin;i++)
                if (lfltSortie<lfltTableau[i])
                {
                    lfltSortie = lfltTableau[i];
                    lintSortie = i;
                }
        }
        else
            return -9999;
    }
    else
        return -9999;
    return lintSortie;
}

int getAccInfo::minPos(float *lfltTableau, int lintTailleTableau, int lintDebut, int lintFin)
{
    float lfltSortie = -9999;
    int lintSortie = -9999;
    if ((lintFin>lintDebut) & ((lintFin-lintDebut)<=lintTailleTableau))
    {
        if (lintTailleTableau>0)
        {
            lfltSortie = lfltTableau[lintDebut];
            lintSortie = lintDebut;
            for (int i=lintDebut+1;i<lintFin;i++)
                if (lfltSortie<lfltTableau[i])
                {
                    lfltSortie = lfltTableau[i];
                    lintSortie = i;
                }
        }
        else
            return -9999;
    }
    else
        return -9999;
    return lintSortie;
}

float getAccInfo::getOCRA4DBSession(int lintNbAT)
{
    return lintNbAT*(60.0/getDureeTransmission())/getOCRA_RTA();
}

float getAccInfo::getOCRA_RTA()
{
    return gfltOcraRTE;
}

void getAccInfo::setOcraRta(float lfltOCRA_RTA)
{
    gfltOcraRTE = lfltOCRA_RTA;
}

float getAccInfo::getDbNiveauDeRisque(int lintSession, bool lblMontreGauche)
{
    //niveau de risaue
    //duree de travail :
    //la duree chaude 2h
    float lfltMaxTiming = 120.0;
    int lintDuree = getCurrentSessionDuration(lintSession);
    int lintIndividu = getAgentIdFromSession(lintSession);
    float lfltRisqueTampon = (lintDuree/60.0)/(lfltMaxTiming); //TODO au bout de combien de temps ca commence a etre chaud? disons 120 minutes pour le moment
    //qDebug() << "risque liee au Temps : " + QString::number(lfltRisqueTampon);
    //age
    int lintAge  = getIndividuAge(lintIndividu);
    lfltRisqueTampon += (lintAge-16)/(54-16); //TODO Age maxi et age mini correspondant au risque
    //qDebug() << "risque liee a l'Age : " + QString::number(lfltRisqueTampon);
    // nombre de mouvement total (AT)
    int lintNbTotalMVT = getSessionTotalMVT(lintSession,lblMontreGauche);

    lfltRisqueTampon +=lintNbTotalMVT/(getOCRA_RTA()*lfltMaxTiming); // au dela de combien d'actions technique c est chaud ? je prend le nombre d'action technique RTA et le multiplie par le temps ou c'est chaud
    //qDebug() << "risque liee au total de MVT : " + QString::number(lfltRisqueTampon);
    //le rythme moyen
    float lfltRythme = getSessionRythmeMoyenMVT(lintSession,lblMontreGauche);
    //qDebug() << "sortie de  getSessionRythmeMoyenMVT: " + QString::number(lfltRythme);
    lfltRisqueTampon += lfltRythme/60.0;
    //qDebug() << "risque liee au Rythme : " + QString::number(lfltRisqueTampon);

    // le rythme cardiaque
    // TODO

    // le genre
    // TODO

    // le niveau des vibrations
    // TODO

    // le nombre d'amplitudes rouges et jaune
    // TODO

    // le nombre de charges lourdes
    // TODO

    lfltRisqueTampon =  100*lfltRisqueTampon/5 ;
    qDebug() << "risque normalise : " + QString::number(lfltRisqueTampon);
    return lfltRisqueTampon;
}

bool getAccInfo::deletePreviousSessions()
{
    if (!mydb.open())
        return false;
    else
    {
        QSqlQuery sqry(mydb);
        QSqlQuery sqrDel(mydb);
        QString lstQuery = "Select Id From vaucheptms.sessions where Duree is null";
        //qDebug() <<lstQuery;
        if (sqry.exec(lstQuery))
        {
            while (sqry.next()){
                lstQuery ="DELETE from vaucheptms.sessions where Id="+sqry.value(0).toString();
                //qDebug() <<lstQuery;
                sqrDel.exec(lstQuery);
            }
            return true;
        }
        else
            qDebug() << lstQuery;
        return false;
    }
}

bool getAccInfo::cancelActiveSessions()
{
    if (!mydb.open())
        return false;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "UPDATE vaucheptms.sessions SET Actif = 0 where Actif>0";
        //qDebug() <<lstQuery;
        if (sqry.exec(lstQuery))
            return true;
        else
            qDebug() << lstQuery;
        return false;
    }
}

int getAccInfo::getCurrentSessionDuration(int lintSession)
{
    if (!mydb.open())
        return -1;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT max(time_offset) FROM vaucheptms.enregistrements where Session="+QString::number(lintSession);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                qDebug()<<sqry.value(0);
                return sqry.value(0).toInt()/1000;//pour l'avoir en secondes
            }
        }
        else
            qDebug() << lstQuery;
        return -1;
    }
}

int getAccInfo::getAgentIdFromSession(int lintSession)
{
    //SELECT Identite FROM vaucheptms.sessions where Id=82;
    if (!mydb.open())
        return -1;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT Identite FROM vaucheptms.sessions where Id="+QString::number(lintSession);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toInt();//pour l'avoir en secondes
            }
        }
        else
            qDebug() << lstQuery;
        return -1;
    }
}

unsigned int getAccInfo::drawGraphOnPDF(QPainter &painter, unsigned int luintTopGraph, int lintShift, int lintGraphWidth, int lintGraphHeight, int xmax, int ymax,  int ymin,  int xmin,
                                        QString strLabel1, QString strLabel2,
                                         int lintNumberYLine, int lintNumberXLine, int lintMargin, int lintTextOffset,  int lintLegendWidth, int lintLegendHeight, int lintLegendMargin, int lintLegendInternalMargin, int lintLegendInternalVerticalMargin, int lintLegendLengthTrait, int lintLegendInterline)
{
    /*
    //Paramètres du graphique
    int lintShift = 180;
    int lintGraphWidth = 8000;
    int lintGraphHeight = 3000;
    int lintNumberYLine = 7;
    int lintNumberXLine = 6;
    int lintMargin = 25;
    int lintTextOffset = 100;
    int ymin=0, ymax=14;
    int xmin=0, xmax=20;
    unsigned int luintTopGraph = luintTopPageOffset;
    // Paramètre de l'encadré de légende
    int lintLegendWidth = 1250, lintLegendHeight=480, lintLegendMargin = 45, lintLegendInternalMargin = 50, lintLegendInternalVerticalMargin = 150;
    int lintLegendLengthTrait = 300, lintLegendInterline = 200;*/
    unsigned int luintTopPageOffset = luintTopGraph;

    if (xmax<10){
        lintNumberXLine = xmax;
    }
    else if (xmax<20){
        lintNumberXLine = xmax/2;
    }
    else if (xmax<50){
        lintNumberXLine = xmax/10;
    }
    else if (xmax<120){
        lintNumberXLine = xmax/15;
    }
    else if (xmax<240){
        lintNumberXLine = xmax/30;
    }
    else
        lintNumberXLine = xmax/60;

    painter.setBrush(Qt::lightGray);
    painter.drawRect(lintShift+lintMargin,luintTopPageOffset,lintGraphWidth,lintGraphHeight);
    // Affichage du bloc légende
    painter.setBrush(Qt::white);
    painter.drawRect(lintShift+lintMargin + lintGraphWidth - lintLegendWidth -lintLegendMargin,
                     luintTopPageOffset+lintLegendMargin,
                     lintLegendWidth,
                     lintLegendHeight);

    painter.setBrush(Qt::NoBrush);
    //Crée un feutre bleu pour tracer le graphique bras droit
    QPen pen(Qt::blue, 20);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.drawLine(lintShift+lintMargin + lintGraphWidth - lintLegendWidth -lintLegendMargin + lintLegendInternalMargin,
                     luintTopPageOffset+lintLegendMargin + lintLegendInternalVerticalMargin,
                     lintShift+lintMargin + lintGraphWidth - lintLegendWidth -lintLegendMargin + lintLegendInternalMargin + lintLegendLengthTrait,
                     luintTopPageOffset+lintLegendMargin + lintLegendInternalVerticalMargin
                     );
    pen.setBrush(Qt::yellow);
    painter.setPen(pen);
    painter.drawLine(lintShift+lintMargin + lintGraphWidth - lintLegendWidth -lintLegendMargin + lintLegendInternalMargin,
                     luintTopPageOffset+lintLegendMargin + lintLegendInterline + lintLegendInternalVerticalMargin,
                     lintShift+lintMargin + lintGraphWidth - lintLegendWidth -lintLegendMargin + lintLegendInternalMargin + lintLegendLengthTrait,
                     luintTopPageOffset+lintLegendMargin + lintLegendInterline + lintLegendInternalVerticalMargin
                     );
    // On change de couleur et de police

    painter.setPen(Qt::black);
    painter.setFont(QFont("Helvetica",8));
    // Ecrite le texte
    painter.drawText(lintShift + lintMargin + lintGraphWidth - lintLegendWidth - lintLegendMargin + lintLegendLengthTrait + 2 * lintLegendInternalMargin,
                     luintTopPageOffset + lintLegendMargin + lintLegendInternalVerticalMargin - lintTextOffset ,
                     1000,1000,Qt::AlignLeft,strLabel1 );
    painter.drawText(lintShift + lintMargin + lintGraphWidth - lintLegendWidth - lintLegendMargin + lintLegendLengthTrait + 2 * lintLegendInternalMargin,
                     luintTopPageOffset+lintLegendMargin + lintLegendInternalVerticalMargin + lintLegendInterline - lintTextOffset ,
                     1000,1000,Qt::AlignLeft,strLabel2);
    // Le haut du texte doit être légèrement décalé pour être à peu près au milieu de la ligne et pas en dessous
    luintTopPageOffset-=lintTextOffset;
    QRect r2 (0,0,lintShift, lintGraphHeight/lintNumberYLine);
    QRect boundingRect;
    r2.moveTop(luintTopPageOffset);
    for (int i=lintNumberYLine;i>=0;i--){ // Lignes horizontales
        painter.drawText(r2,Qt::AlignRight, QString::number(i*(ymax-ymin)/lintNumberYLine));
        painter.setPen(Qt::white);
        painter.drawLine(lintShift+5,luintTopPageOffset+lintTextOffset,lintGraphWidth+lintShift,luintTopPageOffset+lintTextOffset);
        painter.setPen(Qt::black);

        luintTopPageOffset+=lintGraphHeight/lintNumberYLine;
        r2.moveTop(luintTopPageOffset);
    }
    for (int i=0;i<lintNumberXLine;i++){// Lignes verticales
        painter.setPen(Qt::white);
        painter.drawLine(lintShift+5 +i*lintGraphWidth/lintNumberXLine,
                         luintTopGraph,
                         lintShift+5 +i*lintGraphWidth/lintNumberXLine,
                         luintTopGraph+lintGraphHeight);
        painter.setPen(Qt::black);
        r2.moveTo(lintShift-lintMargin +(i+1)*lintGraphWidth/lintNumberXLine,
                  luintTopGraph+lintGraphHeight);
        painter.drawText(r2,Qt::AlignLeft,QString::number((i+1)*(xmax-xmin)/lintNumberXLine),&boundingRect);
    }
    painter.drawText(lintShift+ boundingRect.width() +lintGraphWidth,
                     luintTopGraph+lintGraphHeight, 1000,1000,Qt::AlignLeft," Minutes");
    return luintTopPageOffset;
}

int getAccInfo::getSessionTotalMVT(int lintSession, bool lblMontreGauche){
    if (!mydb.open())
        return 0;
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT sum(nb_actions) FROM vaucheptms.enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "Session="+QString::number(lintSession);

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

int getAccInfo::getSessionTotalObjets(int lintSession)
{
    if (!mydb.open())
        return 0;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT sum(nb_objets) FROM vaucheptms.enregistrements where Session="+QString::number(lintSession);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

int getAccInfo::getSessionTotalCharges(int lintSession)
{
    if (!mydb.open())
        return 0;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT sum(nb_charges_lourdes) FROM vaucheptms.enregistrements where Session="+QString::number(lintSession);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

float getAccInfo::getSessionMeanRepetitivite(int lintSession, bool lblMontreGauche)
{
    //repetitivite
    if (!mydb.open())
        return 0;
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT avg(repetitivite) FROM vaucheptms.enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "Session="+QString::number(lintSession);

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toFloat();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

float getAccInfo::getSessionMeanOCRA(int lintSession, bool lblMontreGauche)
{
    //indice_OCRA
    if (!mydb.open())
        return 0;
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT avg(indice_OCRA) FROM vaucheptms.enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "Session="+QString::number(lintSession);
        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toFloat();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

float getAccInfo::getSessionMeanRisque(int lintSession, bool lblMontreGauche)
{
    //niveau_risque
    if (!mydb.open())
        return 0;
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT avg(niveau_risque) FROM vaucheptms.enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "Session="+QString::number(lintSession);

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toFloat();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

float getAccInfo::getSessionRythmeMoyenMVT(int lintSession,bool lblMontreGauche)
{
    float lfltRetour = 0;//;
    float lfltSessionDuration = getCurrentSessionDuration(lintSession);
    //qDebug() << "La durée de la session (time_offset) est : " + QString::number(lfltSessionDuration);
    if (lfltSessionDuration>0)
         lfltRetour = getSessionTotalMVT(lintSession,lblMontreGauche)/(lfltSessionDuration/60.0);
    //qDebug() << "Le nombre total de mvt  est : " << lfltRetour;
    return lfltRetour;
}

float getAccInfo::getSessionLastRyhtm(int lintSession, bool lblMontre)
{
    float lfltRetour = 0;//;
    float lfltSessionDuration = getCurrentSessionDuration(lintSession);
    if (lfltSessionDuration>0)
         lfltRetour = getCurrentSessionLastAT(lintSession,lblMontre)/(lfltSessionDuration/60.0);
    return lfltRetour;
}

int getAccInfo::getCurrentSessionId(int lintIndividu)
{
    // SELECT Id FROM vaucheptms.sessions where Actif=2 and Identite=3 ;
    if (!mydb.open())
        return -9;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT Id FROM vaucheptms.sessions where Actif=2 and Identite="+QString::number(lintIndividu);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -1;
    }
}

bool getAccInfo::deleteSession(int lintSession)
{
    if (!mydb.open())
        return false;
    else
    {
        QSqlQuery sqry(mydb);
        QSqlQuery sqrEnregistrements(mydb);
        QString lstQuery = "SELECT idEnregistrement from vaucheptms.enregistrements where Session="+QString::number(lintSession);
        if (sqry.exec(lstQuery))
        {
            while (sqry.next()){
                lstQuery ="DELETE from vaucheptms.enregistrements where idEnregistrement="+sqry.value(0).toString();
                //qDebug() <<lstQuery;
                sqrEnregistrements.exec(lstQuery);
            }
        }
        else
            qDebug() << lstQuery;
        lstQuery = "DELETE FROM vaucheptms.sessions where Id="+QString::number(lintSession);
        //qDebug()<<lstQuery;
        if (sqry.exec(lstQuery))
            return true;
        else
            return false;
    }
}

void getAccInfo::setDureeTransmission(int lintNbSecondes)
{
    gIntDureeEnSecondeTransmission = lintNbSecondes;
}

float getAccInfo::getFltRythmeInstantMVT(bool lblMontre)
{
    return gIntNbMVT[lblMontre]*60/getDureeTransmission();
}

float getAccInfo::getFltRythmeMoyenMVT(bool lblMontre)
{
    if (getDureeTotaleEnregistrementEnMinutes())
        return gIntNbTotalMVT[lblMontre]*60/(gintNbTotalTransmission*getDureeTransmission());
    else
        return getFltRythmeInstantMVT(lblMontre);
}

int getAccInfo::getDureeTotaleEnregistrement()
{
    return gintNbTotalTransmission*getDureeTransmission();
}

int getAccInfo::getDureeTotaleEnregistrementEnMinutes()
{
    return gintNbTotalTransmission*getDureeTransmission()/60;
}

int getAccInfo::getIntNbTotalTransmission()
{
    return gintNbTotalTransmission;
}

int getAccInfo::getIntNbSuccesTransmission()
{
    return gIntNbSuccesTransmission;
}

int getAccInfo::getTempsTravail()
{
    return gIntNbSuccesTransmission*getDureeTransmission();
}

int getAccInfo::getIntNbTotalMVT(bool lblMontre)
{
    return gIntNbTotalMVT[lblMontre];
}

int getAccInfo::getTblIntNbMVT(int lintIndex, bool lblMontre)
{
    if (lintIndex<NBMAXTransmissionS)
        return gTblIntNbMVT[lintIndex][lblMontre];
    else
        return -9999;
}

bool getAccInfo::startTransmission(int lintIndividu)
{

    return true;
}

QString getAccInfo::getDBValue(QString qstrTable, QString qstrRow, int lintIndex)
{
    if (!mydb.open())
    {
        return "-9000";
    }
    else
    {
        //qDebug() << "Database is opened!";
        QSqlQuery sqry(mydb);
        //qDebug() <<"SELECT "+qstrRow+" FROM "+ qstrTable + " WHERE Id="+QString::number(lintIndex);
        if (sqry.exec("SELECT "+qstrRow+" FROM "+ qstrTable + " WHERE Id="+QString::number(lintIndex)))
        {
            int lintCount=0;
            sqry.first();
            {
                lintCount++;
                //qDebug() << sqry.value(0).toString();
                return sqry.value(0).toString();
            }
        }
        else{
            qDebug() << "mmh something got wrong while retrieving the data";
            QString lstQuery = "SELECT "+qstrRow+" FROM "+ qstrTable + " WHERE Id="+QString::number(lintIndex);
            //qDebug() << "check : "<< lstQuery;
        }
        return "-9999";
    }
}

QString getAccInfo::getMontreSN(int lintIndividu, bool lblMontreGauche)
{
    if (!mydb.open())
    {
        return "-9000";
    }
    else
    {
        //qDebug() << "Database is opened!";
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT montres.codeID from identites, montres where identites.Id="+QString::number(lintIndividu);
        //qDebug() << lstQuery;
        if (lblMontreGauche)
            lstQuery += " and identites.Montre_Gauche=montres.Id";
        else
            lstQuery += " and identites.Montre_Droit=montres.Id";
        if (sqry.exec(lstQuery))
        {
            int lintCount=0;
            if (sqry.first())
            {
                lintCount++;
                return sqry.value(0).toString();
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

int getAccInfo::getIndividuAge(int lintIndividu)
{
    QString lstrDateDeNaissance = getDBValue("identites","Date_de_naissance",lintIndividu);
    //qDebug() <<  lstrDateDeNaissance.split("/")[0];
    if (lstrDateDeNaissance.length()>4){
        QString lstrYear = lstrDateDeNaissance.split("-")[0], lstrMonth=lstrDateDeNaissance.split("-")[1], lstrDay=lstrDateDeNaissance.split("-")[2];
        QDate ldateNaissance(lstrYear.toInt(),lstrMonth.toInt(),lstrDay.toInt());
        //qDebug() << ldateNaissance.daysTo(QDate::currentDate());
        return ldateNaissance.daysTo(QDate::currentDate())/365.25;
    }
    else
        return 0;
}

int getAccInfo::getNombreAgents()
{
    if (!mydb.open())
        return -9000;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT  count(Id) from identites";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toInt();//.toString();
            }
        }
        else
            qDebug() << lstQuery;
        return -9999;
    }
}

QString getAccInfo::getAgentNomLst(int lintIndex, int lintStatus)
{
    if (!mydb.open())
        return "-9000";
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT Nom from identites Order by Id limit 1 offset "+QString::number(lintIndex);
        //qDebug() << lstQuery;
        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toString();
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

int getAccInfo::getAgentId(int lintIndex)
{
    if (!mydb.open())
        return -9000;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT Id from identites Order by Id limit 1 offset "+QString::number(lintIndex);
        //qDebug() << lstQuery;
        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9999;
    }
}

int getAccInfo::getAgentId(QString strSerialNo)
{
    if (!mydb.open())
        return -9000;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "select identites.Id as Id from identites, montres where (identites.Montre_Droit=montres.Id OR identites.Montre_Gauche=montres.Id) AND montres.codeID='"+strSerialNo+"'";
        //qDebug() << lstQuery;
        if (sqry.exec(lstQuery)){
            if (sqry.first())
                return sqry.value(0).toInt();
        }
        else
            qDebug() << lstQuery;
        return -9999;
    }
}

int getAccInfo::getAgentIdFromMessage(QString strMessage)
{
    if (!mydb.open())
        return -9000;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "select identites.Id as Id from identites, montres where (identites.Montre_Droit=montres.Id OR identites.Montre_Gauche=montres.Id) AND montres.codeID='"+strMessage.split(" ")[1]+"'";
        //qDebug() << lstQuery;
        if (sqry.exec(lstQuery)){
            if (sqry.first())
                return sqry.value(0).toInt();
        }
        else
            qDebug() << lstQuery;
        return -9999;
    }
}

QString getAccInfo::getCoreMessage(QString strSerialNo, bool lblOffset)
{
    return strSerialNo.split(' ')[lblOffset];
}

int getAccInfo::getAgentStatus(int lintIndex)
{
    if (!mydb.open())
        return -9000;
    else
    {
        //regarde le lastupdate des montres, si aucune des 2 n'a un lastupdate plus recent de 60'000ms alors il retourne de toute facon 0
        QSqlQuery luQuery(mydb);
        //QString lstQuery
        if (luQuery.exec("Select count(LastActif) from identites, montres where (Montre_Droit=montres.Id OR Montre_Gauche=montres.Id) AND (LastActif+60000)>(UNIX_TIMESTAMP(CURRENT_TIMESTAMP)*1000) AND identites.Id="+QString::number(lintIndex))){
            if (luQuery.first()){
                if (luQuery.value(0).toInt()==0)
                    return 0;
                else
                    qDebug() << "Agent #"+QString::number( lintIndex)+" passed count timing !!";
            }
        }
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT Actif from sessions where Identite="+QString::number(lintIndex)+" order by date_debut desc limit 1";
        //qDebug() << lstQuery;
        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9999;
    }
}

int getAccInfo::getNombreSessions(int lintIndex)
{
    if (!mydb.open())
        return -9000;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT count(distinct sessions.Id) from sessions,enregistrements where sessions.Duree>100 and enregistrements.Session=sessions.Id AND Identite="+QString::number(lintIndex);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                //qDebug() << sqry.value(0).toString();
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9999;
    }
}

QString getAccInfo::getSessiondDate(int lintIndividu, int lintIndex)
{
    if (!mydb.open())
        return "-9000";
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT date_debut from sessions,enregistrements where sessions.Duree>100 AND Identite="+QString::number(lintIndividu)+" and enregistrements.Session=sessions.Id GROUP BY enregistrements.Session order by date_debut desc limit 1 offset "+QString::number(lintIndex);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                //qDebug() << sqry.value(0).toString();
                QDateTime timestamp;
                timestamp.setTime_t(sqry.value(0).toULongLong()/1000);
                return timestamp.toString(Qt::SystemLocaleShortDate);
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

QString getAccInfo::getSessionDate(int lintSession)
{
    if (!mydb.open())
        return "-9000";
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT date_debut from sessions where sessions.Duree>100 AND sessions.Id="+QString::number(lintSession);

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                QDateTime timestamp;
                timestamp.setTime_t(sqry.value(0).toULongLong()/1000);
                return timestamp.toString(Qt::SystemLocaleShortDate);
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

QString getAccInfo::getFileSessionDate(int lintIndex)
{
    QDateTime timestamp;
    timestamp.setTime_t(tblFileSessions[lintIndex].ulngTimeStamp/1000);
    return timestamp.toString(Qt::SystemLocaleShortDate);
}

QString getAccInfo::getSessionDuration(int lintIndividu, int lintIndex)
{
    if (!mydb.open())
        return "-9000";
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT sessions.Duree from sessions,enregistrements where sessions.Duree>100 AND Identite="+QString::number(lintIndividu)+" and enregistrements.Session=sessions.Id GROUP BY enregistrements.Session order by date_debut desc limit 1 offset "+QString::number(lintIndex);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                if (sqry.value(0).toULongLong()<60000)
                    return QString::number(sqry.value(0).toULongLong()/1000)+"sec.";
                else if (sqry.value(0).toULongLong()<3600000)
                    return QString::number(sqry.value(0).toULongLong()/60000)+"min.";
                else
                    return QString::number(sqry.value(0).toULongLong()/3600000)+"h...";;
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

QString getAccInfo::getStrSessionDuration(int lintSession)
{
    if (!mydb.open())
        return "-9000";
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT sessions.Duree from sessions,enregistrements where sessions.Duree>100 AND sessions.Id="+QString::number(lintSession);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                if (sqry.value(0).toULongLong()<60000)
                    return QString::number(sqry.value(0).toULongLong()/1000)+" sec.";
                else if (sqry.value(0).toULongLong()<3600000)
                    return QString::number(sqry.value(0).toULongLong()/60000)+" min.";
                else
                    return QString::number(sqry.value(0).toULongLong()/3600000)+" h...";;
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

int getAccInfo::getFileSessionDurationInSec(int lintIndex)
{
    return tblFileSessions[lintIndex].ushrtDuration;
}

QString getAccInfo::getFileSessionDuration(int lintIndex){
    if (tblFileSessions[lintIndex].ushrtDuration<60)
        return QString::number(tblFileSessions[lintIndex].ushrtDuration)+"sec.";
    else if (tblFileSessions[lintIndex].ushrtDuration<3600)
        return QString::number(tblFileSessions[lintIndex].ushrtDuration/60)+"min.";
    else
        return QString::number(tblFileSessions[lintIndex].ushrtDuration/3600)+"h...";
}

int getAccInfo::getFileSessionNumber(int lintIndex){
    return tblFileSessions[lintIndex].uchrFileNumber;
}

QString getAccInfo::getFileSessionTimestamp(int lintIndex)
{
    return QString::number(tblFileSessions[lintIndex].ulngTimeStamp);
}

int getAccInfo::getSessionDuration(int lintSession)
{
    if (!mydb.open())
        return -9;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT sessions.Duree from sessions where sessions.Id="+QString::number(lintSession);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first())
                    return sqry.value(0).toULongLong()/60000;
        }
        else
            qDebug() << lstQuery;
        return -1;
    }
}

int getAccInfo::getSessionID(int lintIndividu, int lintIndex)
{
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT sessions.Id from sessions,enregistrements where sessions.Duree>100 AND Identite="+QString::number(lintIndividu)+" and enregistrements.Session=sessions.Id GROUP BY enregistrements.Session order by date_debut desc limit 1 offset "+QString::number(lintIndex);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9;
    }
}

int getAccInfo::getSessionNbEnregistrements(int lintSession, bool lblMontreGauche)
{
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT count(*) from enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "enregistrements.Session="+QString::number(lintSession);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9;
    }
}

int getAccInfo::getSessionNbEnregistrementsMinutes(int lintSession, bool lblMontreGauche)
{

    return getSessionNbEnregistrements(lintSession,lblMontreGauche)/ (60/gIntDureeEnSecondeTransmission);

}

int getAccInfo::getSessionValueAT(int lintSession, int lintIndex, bool lblMontreGauche)
{
    //SELECT nb_actions from enregistrements where enregistrements.Session=93 limit 1 offset 0;
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT nb_actions from enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "enregistrements.Session="+QString::number(lintSession)+" limit 1 offset "+QString::number(lintIndex);
        qDebug()<<lstQuery;
        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9;
    }
}

int getAccInfo::getSessionValueATParMinute(int lintSession, int lintIndex, bool lblMontreGauche)
{
    //qDebug()<<"getSessionValueATParMinute request :" << lintSession <<", "<< lintIndex<<", " <<lblMontreGauche;
    float tmpValue=0;
    int lintNb = 0;
    for (int i=lintIndex;i<lintIndex+(60/gIntDureeEnSecondeTransmission);i++){
        int  lintTmpValue = getSessionValueAT(lintSession,i,lblMontreGauche);
        if ((lintTmpValue!=-9) && ((lintTmpValue!=-99))){
            tmpValue += lintTmpValue;
            lintNb++;
        }
        else
            break;
    }
    tmpValue /= lintNb;
    return qFloor(tmpValue);
}

int getAccInfo::getSessionValueRiskParMinute(int lintSession, int lintIndex, bool lblMontreGauche)
{
    float tmpValue=0;
    int lintNb = 0;
    for (int i=lintIndex;i<lintIndex+(60/gIntDureeEnSecondeTransmission);i++){
        int  lintTmpValue = getSessionValueRisk(lintSession,i,lblMontreGauche);
        if ((lintTmpValue!=-9) && ((lintTmpValue!=-99))){
            tmpValue += lintTmpValue;
            lintNb++;
        }
        else
            break;
    }
    tmpValue /= lintNb;
    return qFloor(tmpValue);
}

int getAccInfo::getSessionValueOCRAParMinute(int lintSession, int lintIndex, bool lblMontreGauche)
{
    float tmpValue=0;
    int lintNb = 0;
    for (int i=lintIndex;i<lintIndex+(60/gIntDureeEnSecondeTransmission);i++){
        int  lintTmpValue = getSessionValueOCRA(lintSession,i,lblMontreGauche);
        if ((lintTmpValue!=-9) && ((lintTmpValue!=-99))){
            tmpValue += lintTmpValue;
            lintNb++;
        }
        else
            break;
    }
    tmpValue /= lintNb;
    return qFloor(tmpValue);
}

int getAccInfo::getCurrentSessionLastAT(int lintSession, bool lblMontre)
{
    //SELECT nb_actions, time_offset FROM vaucheptms.enregistrements where Session=93 and  (poignet_gauche=b'0' or poignet_gauche is null)  order by time_offset desc limit 1;
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT nb_actions FROM vaucheptms.enregistrements where Session="+QString::number(lintSession)+" and (poignet_gauche=b'"+ QString::number(lblMontre)+"' or poignet_gauche is null)  order by time_offset desc limit 1";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                //qDebug()<<sqry.value(0).toString();
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9;
    }
}

float getAccInfo::getSessionLastRisk(int lintSession, bool lblMontre)
{
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT niveau_risque FROM vaucheptms.enregistrements where Session="+QString::number(lintSession)+" and (poignet_gauche=b'"+ QString::number(lblMontre)+"' or poignet_gauche is null)  order by time_offset desc limit 1";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){

                return sqry.value(0).toFloat();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

int getAccInfo::getSessionLastObjets(int lintSession, bool lblMontre)
{
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT niveau_risque FROM vaucheptms.enregistrements where Session="+QString::number(lintSession)+" and (poignet_gauche=b'"+ QString::number(lblMontre)+"' or poignet_gauche is null)  order by time_offset desc limit 1";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

int getAccInfo::getSessionLastCharges(int lintSession, bool lblMontre)
{
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT nb_charges_lourdes FROM vaucheptms.enregistrements where Session="+QString::number(lintSession)+" and (poignet_gauche=b'"+ QString::number(lblMontre)+"' or poignet_gauche is null)  order by time_offset desc limit 1";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

int getAccInfo::getSessionValueOCRA(int lintSession, int lintIndex, bool lblMontreGauche)
{
    if (!mydb.open())
        return -99;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT indice_OCRA from enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "enregistrements.Session="+QString::number(lintSession)+" limit 1 offset "+QString::number(lintIndex);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9;
    }
}

int getAccInfo::getSessionValueRisk(int lintSession, int lintIndex, bool lblMontreGauche)
{
    if (!mydb.open())
        return -99;
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT niveau_risque from enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "enregistrements.Session="+QString::number(lintSession)+" limit 1 offset "+QString::number(lintIndex);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9;
    }
}

int getAccInfo::getSessionValueObjets(int lintSession, int lintIndex, bool lblMontreGauche)
{
    if (!mydb.open())
        return -99;
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT nb_objets from enregistrements where ";
        if (lblMontreGauche) lstQuery += "poignet_gauche=1 AND ";
        else lstQuery += "(poignet_gauche is null OR poignet_gauche=0) AND ";
        lstQuery += "enregistrements.Session="+QString::number(lintSession)+" limit 1 offset "+QString::number(lintIndex);

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                return sqry.value(0).toInt();
            }
        }
        else
            qDebug() << lstQuery;
        return -9;
    }
}

bool getAccInfo::setSessionMustStart(int lintIndividu)
{
    if (!mydb.open())
        return false;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "select identites.Montre_Gauche, Montre_Droit from identites where identites.Id="+QString::number(lintIndividu);

        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                lstQuery = "update montres set montres.status='muststart' WHERE Id="+sqry.value(0).toString() + " OR Id=" +sqry.value(1).toString();
                if (sqry.exec(lstQuery))
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
        else
            qDebug() << lstQuery;
        return false;
    }
}

bool getAccInfo::setSessionWouldStop(int lintIndividu)
{
    if (!mydb.open())
        return false;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "select identites.Montre_Gauche, Montre_Droit from identites where identites.Id="+QString::number(lintIndividu);

       //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                lstQuery = "update montres set montres.status='wouldstop' WHERE Id="+sqry.value(0).toString()+ " OR Id=" +sqry.value(1).toString();
                if (sqry.exec(lstQuery))
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
        else
            qDebug() << lstQuery;
        return false;
    }
}

bool getAccInfo::sendMessage(int lintIndividu, QString strMessage)
{
    if (!mydb.open())
        return false;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "select identites.Montre_Gauche from identites where identites.Id="+QString::number(lintIndividu);

        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                lstQuery = "update montres set montres.status='msg="+ strMessage +"' WHERE Id="+sqry.value(0).toString();
                if (sqry.exec(lstQuery))
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
        else
            qDebug() << lstQuery;
        return false;
    }
}

int getAccInfo::getNbDechets(bool lblMontre)
{
    return gIntNbDechets[lblMontre];
}

int getAccInfo::getNbDechetsTotal(bool lblMontre)
{
    return gIntNbTotalDechets[lblMontre];
}

int getAccInfo::getNbMontres()
{
    if (!mydb.open())
        return 0;
    else{
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT count(codeID) AS NB FROM vaucheptms.montres where status <> 'recording'";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery)){
            if (sqry.first()){
                gintNbTotalWatches = sqry.value(0).toInt();
                return gintNbTotalWatches;
            }
        }
        else
            qDebug() << lstQuery;
        return 0;
    }
}

QString getAccInfo::getNextWatch()
{
    if (!mydb.open())
        return "-9000";
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "SELECT codeID FROM vaucheptms.montres  where status <> 'recording' limit 1 offset " + QString::number(gintCurrentWatchOffset);
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                if (gintCurrentWatchOffset<(gintNbTotalWatches-1))
                    gintCurrentWatchOffset++;
                else
                    gintCurrentWatchOffset = 0;
                //qDebug() << gintCurrentWatchOffset;
                return sqry.value(0).toString();
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

bool getAccInfo::setAgentWatch(int lintIndividu, QString lstrWatchID, bool lblGauche)
{
    if (!mydb.open())
        return false;
    else
    {
        // recupere l'id a partir de lstrWatchID ssi le status n'est pas recording
        QSqlQuery sqry(mydb);
        QSqlQuery query(mydb);
        QString lstQuery = "SELECT Id FROM vaucheptms.montres where status <> 'recording' AND codeID='"+lstrWatchID+"'";
        //qDebug() << lstQuery;
        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                // commence par virer toutes les valeurs precedentes :
                lstQuery= "update identites set Montre_Gauche=null where Montre_Gauche="+sqry.value(0).toString();
                //qDebug() << lstQuery;
                query.exec(lstQuery);
                lstQuery = "update identites set Montre_Droit=null where Montre_Droit="+sqry.value(0).toString();
                //qDebug() << lstQuery;
                query.exec(lstQuery);
                //Et maintenant ajoute l'ID au bon endroit
                if (lblGauche==true)
                    lstQuery = "update identites set Montre_Gauche="+sqry.value(0).toString() +" where Id=" + QString::number(lintIndividu);
                else
                    lstQuery = "update identites set Montre_Droit="+sqry.value(0).toString() +" where Id=" + QString::number(lintIndividu);
                //qDebug() << lstQuery;
                query.exec(lstQuery);
                return true;
            }
            else
                return false;
        }
        else
            return false;
    }
    return false;
}

bool getAccInfo::getWatchStatus(QString lstrSerialNo)
{
    //"Select count(LastActif) from  montres where (LastActif+60000)>(UNIX_TIMESTAMP(CURRENT_TIMESTAMP)*1000) AND montres.Id="
    if (!mydb.open())
        return false;
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "Select count(LastActif) from  montres where (LastActif+60000)>(UNIX_TIMESTAMP(CURRENT_TIMESTAMP)*1000) AND montres.codeID='"+lstrSerialNo+"'";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                //qDebug() << gintCurrentWatchOffset;
                return sqry.value(0).toBool();
            }
        }
        else
            qDebug() << lstQuery;
        return false;
    }

}

QString getAccInfo::getAgentNom(QString strMessage)
{

    if (!mydb.open())
        return "-9000";
    else
    {
        QSqlQuery sqry(mydb);
        QString lstQuery = "select Nom, Prenom from identites, montres where (Montre_Droit=montres.Id or Montre_Gauche=montres.Id) and montres.codeID='"+strMessage.split(" ")[1]+"'";
        //qDebug() << lstQuery;

        if (sqry.exec(lstQuery))
        {
            if (sqry.first())
            {
                //qDebug() << gintCurrentWatchOffset;
                return sqry.value(1).toString() + " " + sqry.value(0).toString();
            }
        }
        else
            qDebug() << lstQuery;
        return "-9999";
    }
}

bool getAccInfo::generatePDF(int lintSession)
{
    unsigned int luintTopPageOffset =0;
    QString filename = UPLOADACCPATH+"session.pdf";
    QPdfWriter writer(filename);
    writer.setPageSize(QPagedPaintDevice::A4);
    writer.setPageMargins(QMargins(30,30,30,30));

    QPainter painter(&writer);
    painter.setPen(Qt::black);
    QRect r = painter.viewport();

    painter.setFont(QFont("Times",18, QFont::Black));
    QString strContent = "Session du "+getSessionDate(lintSession) + "  (Id :#"+QString::number(lintSession)+")";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=500;
    r.moveTop(luintTopPageOffset);

    painter.setFont(QFont("Times",16, QFont::Bold));
    strContent = "Durée : "+getStrSessionDuration(lintSession) ;
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=500;
    r.moveTop(luintTopPageOffset);

    QFont lFontTitle("Times",14, QFont::Bold);
    lFontTitle.setUnderline(true);
    painter.setFont(lFontTitle);
    strContent = "Nombre d'Actions Techniques :";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=500;
    r.moveTop(luintTopPageOffset);
    /*
            Dessiner le graphique nombre d'action Techniques
     */
    int lintNbData = getSessionNbEnregistrements(lintSession);
    int lintNbDataG = getSessionNbEnregistrements(lintSession,1);
    int xmax = getSessionDuration(lintSession),xmin=0, ymin=0, ymax=14;
    int lintShift = 180, lintGraphWidth = 8000, lintGraphHeight = 3000;
    unsigned int luintTopPageOffsetTemp = drawGraphOnPDF(painter, luintTopPageOffset,lintShift,lintGraphWidth,lintGraphHeight,xmax,ymax,ymin, xmin);

    QPainterPath path, pathG;
    lintShift += 25;
    path.moveTo(lintShift, luintTopPageOffset+lintGraphHeight);
    if (lintNbData) path.moveTo(lintShift, luintTopPageOffset+lintGraphHeight-getSessionValueAT(lintSession,0)*100);
    for (int i=1;i<lintNbData;i++)
        path.lineTo(lintShift +i*lintGraphWidth/(lintNbData-1), luintTopPageOffset+lintGraphHeight-(getSessionValueAT(lintSession,i)*lintGraphHeight/(ymax-ymin)));

    pathG.moveTo(lintShift, luintTopPageOffset+lintGraphHeight);
    if (lintNbDataG) pathG.moveTo(lintShift, luintTopPageOffset+lintGraphHeight-getSessionValueAT(lintSession,0,1)*100);
    for (int i=0;i<lintNbDataG;i++)
        pathG.lineTo(lintShift +i*lintGraphWidth/(lintNbData-1), luintTopPageOffset+lintGraphHeight-getSessionValueAT(lintSession,i,1)*lintGraphHeight/(ymax-ymin));

    QPen pen(Qt::blue, 20);
    //pen.setBrush(Qt::blue);
    painter.setPen(pen);
    painter.drawPath(path);
    pen.setBrush(Qt::yellow);
    painter.setPen(pen);
    painter.drawPath(pathG);
    pen.setBrush(Qt::black);
    painter.setPen(pen);
    luintTopPageOffset = luintTopPageOffsetTemp;
    r.moveTop(luintTopPageOffset);

    // Ecrire le nombre d'AT et le rythme pour chaque bras
    painter.setFont(QFont("Times",12));
    strContent = "Nombre d'AT du bras Droit : "+QString::number(getSessionTotalMVT(lintSession));
    strContent += " (" + QString::number(getSessionRythmeMoyenMVT(lintSession),'f',1) + " mvt/min)";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=250;
    r.moveTop(luintTopPageOffset);

    painter.setFont(QFont("Times",12));
    strContent = "Nombre d'AT du bras Gauche : "+QString::number(getSessionTotalMVT(lintSession,1));
    strContent += " (" + QString::number(getSessionRythmeMoyenMVT(lintSession,1),'f',1) + " mvt/min)";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=500;
    r.moveTop(luintTopPageOffset);

    /*
     * RISQUES TMS
     */
    painter.setFont(lFontTitle);
    strContent = "Risque TMS :";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=500;
    r.moveTop(luintTopPageOffset);

    ymax = 100;
    luintTopPageOffsetTemp = drawGraphOnPDF(painter, luintTopPageOffset,lintShift,lintGraphWidth,lintGraphHeight,xmax,ymax,ymin, xmin);
    path = QPainterPath();
    path.moveTo(lintShift, luintTopPageOffset+lintGraphHeight);
    if (lintNbData) path.moveTo(lintShift, luintTopPageOffset+lintGraphHeight-getSessionValueRisk(lintSession,0)*100);
    for (int i=1;i<lintNbData;i++)
        path.lineTo(lintShift +i*lintGraphWidth/(lintNbData-1), luintTopPageOffset+lintGraphHeight-(getSessionValueRisk(lintSession,i)*lintGraphHeight/(ymax-ymin)));
    pen.setBrush(Qt::blue);
    painter.setPen(pen);
    painter.drawPath(path);
    pen.setBrush(Qt::black);
    painter.setPen(pen);
    luintTopPageOffset = luintTopPageOffsetTemp;
    r.moveTop(luintTopPageOffset);

    painter.setFont(QFont("Times",12));
    strContent = "Niveau de risque évalué pour le bras Droit : "+QString::number(getSessionMeanRisque(lintSession),'f',1)+"%  > ";
    if (getSessionMeanRisque(lintSession)<33)strContent += "Risque nul ou tres faible ";
    else if (getSessionMeanRisque(lintSession)<33)strContent += "Risque modéré ";
    else strContent += "Risque élevé, prendre des mesures !";

    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=250;
    r.moveTop(luintTopPageOffset);
    painter.setFont(QFont("Times",12));
    strContent = "Niveau de risque évalué pour le bras Gauche : "+QString::number(getSessionMeanRisque(lintSession,1),'f',1)+"% > ";
    if (getSessionMeanRisque(lintSession,1)<33)strContent += "Risque nul ou tres faible ";
    else if (getSessionMeanRisque(lintSession,1)<33)strContent += "Risque modéré ";
    else strContent += "Risque élevé, prendre des mesures !";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=500;
    r.moveTop(luintTopPageOffset);


    writer.newPage();
    luintTopPageOffset = 0;
    r.moveTop(luintTopPageOffset);
    /*
     * INDICE OCRA
     */
    painter.setFont(lFontTitle);
    strContent = "Indice OCRA :";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=500;
    r.moveTop(luintTopPageOffset);

    ymax = 7;
    luintTopPageOffsetTemp = drawGraphOnPDF(painter, luintTopPageOffset,lintShift,lintGraphWidth,lintGraphHeight,xmax,ymax,ymin, xmin);
    //getSessionValueOCRA(lintSessionId,i));
    path = QPainterPath();
    path.moveTo(lintShift, luintTopPageOffset+lintGraphHeight);
    if (lintNbData) path.moveTo(lintShift, luintTopPageOffset+lintGraphHeight-getSessionValueOCRA(lintSession,0)*100);
    for (int i=1;i<lintNbData;i++)
        path.lineTo(lintShift +i*lintGraphWidth/(lintNbData-1), luintTopPageOffset+lintGraphHeight-(getSessionValueOCRA(lintSession,i)*lintGraphHeight/(ymax-ymin)));
    pen.setBrush(Qt::blue);
    painter.setPen(pen);
    painter.drawPath(path);
    pen.setBrush(Qt::black);
    painter.setPen(pen);
    luintTopPageOffset = luintTopPageOffsetTemp;
    r.moveTop(luintTopPageOffset);

    painter.setFont(QFont("Times",12));
    strContent = "Niveau de risque évalué pour le bras Droit : "+QString::number(getSessionMeanOCRA(lintSession),'f',1)+"%";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=250;
    r.moveTop(luintTopPageOffset);
    painter.setFont(QFont("Times",12));
    strContent = "Niveau de risque évalué pour le bras Gauche : "+QString::number(getSessionMeanOCRA(lintSession,1),'f',1)+"%";
    painter.drawText(r,Qt::AlignLeft, strContent);
    luintTopPageOffset+=250;
    r.moveTop(luintTopPageOffset);

    //accInfo.getSessionValueRisk(lintSessionId,i));

    //
    painter.end();

    QString lstrCommand = "/home/eldecog/mailRobot.bash"; //   < email.txt";
    qDebug() << lstrCommand;
    QStringList arguments;
    arguments << "-s" << "Test email depuis flod.systems" << "-t" << "olivier@flod.aero" << "-A" << "/home/eldecog/nodejs/upload/session.pdf";
    QProcess process;
    process.execute(lstrCommand);
    return 1;

}

int getAccInfo::controlEngine(int lintSpeed)
{
    /*QProcess process;
    QString lstrCommand = "echo";
    QStringList arguments;
    arguments << QString::number(lintSpeed) << ">" << "/dev/ttyACM0";
    return process.execute(lstrCommand,arguments);*/
    QSerialPort serial;
    //serial = new QSerialPort(this);
    serial.setPortName("/dev/ttyACM0");
  /*  serial.setBaudRate(QSerialPort::Baud115200);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);*/
    serial.open(QIODevice::ReadWrite);

    QByteArray data=QString::number(lintSpeed).toUtf8().constData();
    serial.write(data);
        if( serial.waitForBytesWritten(100) )
            qDebug() << "sent: " << data;
    serial.close();
    return 0;
}

int getAccInfo::getDureeTransmission()
{
    return gIntDureeEnSecondeTransmission;
}

void getAccInfo::CaracteriseMVT()
{
    for (int i=TAILLEOFF;i<getNbDonnees();i++)
    {
        /*
         * passe en revue le tableau de tick
         * s'il voit un tick alors il recherche dans le tableau des valeurs de combinaison des accélérations
         * la valeur min ou la valeur max, ensuite fait ça une 2eme fois au prochain tick
         */
        int lintDuree = 0;
        float lfltAmplitude = 0;
        int j=gIntNbTotalMVT[0] - gIntNbMVT[0];

        if (gbolChangeDirection[i])
            if ((i>TAILLESEARCH)&(j<NBMAXMVT))
            {
                if (gfltCombinaisonAcc[0][i-1]>gfltCombinaisonAcc[0][i+1])
                {
                    lintDuree = maxPos(gfltCombinaisonAcc[0],MAXFILESIZE, i-TAILLESEARCH, i);
                    lintDuree += minPos(gfltCombinaisonAcc[0],MAXFILESIZE,i,i+TAILLESEARCH);
                    gintDuree[j] = lintDuree;
                    lfltAmplitude = max(gfltCombinaisonAcc[0],MAXFILESIZE, i-TAILLESEARCH, i);
                    lfltAmplitude += min(gfltCombinaisonAcc[0],MAXFILESIZE,i,i+TAILLESEARCH);
                    gfltAmplitude[j++] = lfltAmplitude;
                }
                else
                {
                    lintDuree = maxPos(gfltCombinaisonAcc[0],MAXFILESIZE,i,i+TAILLESEARCH);
                    lintDuree += minPos(gfltCombinaisonAcc[0],MAXFILESIZE, i-TAILLESEARCH, i);
                    gintDuree[j] = lintDuree;
                    lfltAmplitude = max(gfltCombinaisonAcc[0],MAXFILESIZE,i,i+TAILLESEARCH);
                    lfltAmplitude += min(gfltCombinaisonAcc[0],MAXFILESIZE, i-TAILLESEARCH, i);
                    gfltAmplitude[j++] = lfltAmplitude;
                }
            }
    }

}

void getAccInfo::parseServerMessage(QString strMessage)
{

}
