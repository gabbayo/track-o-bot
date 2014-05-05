#include "tracker.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QTimer>
#include <QDesktopServices>

#include "json.h"

Tracker::Tracker() {
  // delay this func so logging system gets ready
  QTimer::singleShot(0, this, SLOT(EnsureAccountIsSetUp()));
}

void Tracker::EnsureAccountIsSetUp() {
  if(!IsAccountSetUp()) {
    logger() << "No account setup. Creating one for you." << endl;
    CreateAndStoreAccount();
  } else {
    logger() << "Account " << Username().toStdString() << " found." << endl;
  }
}

void Tracker::AddResult(GameMode mode, Outcome outcome, GoingOrder order, Class ownClass, Class opponentClass) {
  logger() << "Upload Result" <<
    " Mode: " << MODE_NAMES[mode] <<
    " Outcome: " << OUTCOME_NAMES[outcome] <<
    " Order: " << ORDER_NAMES[order] <<
    " Class: " << CLASS_NAMES[ownClass] <<
    " Opponent: " << CLASS_NAMES[opponentClass] << endl;

  if(mode == MODE_UNKNOWN) {
    logger() << "Mode unknown. Skip result" << endl;
    return;
  }

  if(outcome == OUTCOME_UNKNOWN) {
    logger() << "Outcome unknown. Skip result" << endl;
    return;
  }

  if(order == ORDER_UNKNOWN) {
    logger() << "Order unknown. Skip result" << endl;
    return;
  }

  if(ownClass == CLASS_UNKNOWN) {
    logger() << "Class unknown. Skip result" << endl;
    return;
  }

  if(opponentClass == CLASS_UNKNOWN) {
    logger() << "Opponent unknown. Skip result" << endl;
    return;
  }

  QUrl url("http://webtracker.dev/results.json");
  url.setUserName(Username());
  url.setPassword(Password());

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QtJson::JsonObject result;
  result["coin"]     = (order == ORDER_FIRST);
  result["hero"]     = CLASS_NAMES[ownClass];
  result["opponent"] = CLASS_NAMES[opponentClass];
  result["win"]      = (outcome == OUTCOME_VICTORY);
  result["mode"]     = MODE_NAMES[mode];

  QtJson::JsonObject params;
  params["result"] = result;

  QByteArray data = QtJson::serialize(params);
  QNetworkReply *reply = networkManager.post(request, data);
  connect(reply, SIGNAL(finished()), this, SLOT(AddResultHandleReply()));
}

void Tracker::AddResultHandleReply() {
  QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
  if(reply->error() == QNetworkReply::NoError) {
    logger() << "Result was uploaded succesfully!" << endl;
  } else {
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    logger() << "There was a problem uploading the result. Error: " << reply->error() << " HTTP Status Code: " << statusCode << endl;
  }
}

void Tracker::CreateAndStoreAccount() {
  QUrl url("http://webtracker.dev/users.json");
  QNetworkRequest request(url);
  QNetworkReply *reply = networkManager.post(request, "");
  connect(reply, SIGNAL(finished()), this, SLOT(CreateAndStoreAccountHandleReply()));
}

void Tracker::CreateAndStoreAccountHandleReply() {
  QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
  if(reply->error() == QNetworkReply::NoError) {
    logger() << "Account creation was successful!" << endl;

    QByteArray jsonData = reply->readAll();

    bool ok;
    QtJson::JsonObject user = QtJson::parse(jsonData, ok).toMap();

    if(!ok) {
      logger() << "Couldn't parse response" << endl;
    } else {
      logger() << "Welcome " << user["username"].toString().toStdString() << endl;

      QSettings settings;
      settings.setValue("username", user["username"].toString());
      settings.setValue("password", user["password"].toString());
    }
  } else {
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    logger() << "There was a problem creating an account. Error: " << reply->error() << " HTTP Status Code: " << statusCode << endl;
  }
}

void Tracker::OpenProfile() {
  QUrl url("http://webtracker.dev/one_time_auth.json");
  url.setUserName(Username());
  url.setPassword(Password());
  QNetworkRequest request(url);
  QNetworkReply *reply = networkManager.post(request, "");
  connect(reply, SIGNAL(finished()), this, SLOT(OpenProfileHandleReply()));
}

void Tracker::OpenProfileHandleReply() {
  QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
  if(reply->error() == QNetworkReply::NoError) {
    QByteArray jsonData = reply->readAll();

    bool ok;
    QtJson::JsonObject response = QtJson::parse(jsonData, ok).toMap();

    if(!ok) {
      logger() << "Couldn't parse response" << endl;
    } else {
      QString url = response["url"].toString();
      QDesktopServices::openUrl(QUrl(url));
    }
  } else {
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    logger() << "There was a problem creating an auth token. Error: " << reply->error() << " HTTP Status Code: " << statusCode << endl;
  }
}

QString Tracker::Username() {
  return settings.value("username").toString();
}

QString Tracker::Password() {
  return settings.value("password").toString();
}

void Tracker::SetUsername(const QString& username) {
  settings.setValue("username", username);
}

void Tracker::SetPassword(const QString& password) {
  settings.setValue("password", password);
}

bool Tracker::IsAccountSetUp() {
  return settings.contains("username") && settings.contains("password");
}
