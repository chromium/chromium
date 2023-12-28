// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_push_notification_handler.h"
#include "chrome/browser/push_notification/push_notification_service_factory.h"
#include "components/push_notification/push_notification_service.h"

NearbyInternalsPushNotificationHandler::NearbyInternalsPushNotificationHandler(
    content::BrowserContext* context)
    : context_(context) {}

NearbyInternalsPushNotificationHandler::
    ~NearbyInternalsPushNotificationHandler() = default;

void NearbyInternalsPushNotificationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "AddPushNotificationClient",
      base::BindRepeating(&NearbyInternalsPushNotificationHandler::
                              HandleAddPushNotificationClient,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "InitializePushNotificationHandler",
      base::BindRepeating(&NearbyInternalsPushNotificationHandler::Initialize,
                          base::Unretained(this)));
}

void NearbyInternalsPushNotificationHandler::OnJavascriptAllowed() {}

void NearbyInternalsPushNotificationHandler::OnJavascriptDisallowed() {}

void NearbyInternalsPushNotificationHandler::Initialize(
    const base::Value::List& args) {
  AllowJavascript();
}

// TODO(b/306399642): Once the `PushNotificationClient` base class is created,
// this function will be used to retrieve the service and then add
// `NearbyInternalsPushNotificationHandler` as a `PushNotificationClient` to
// `PushNotificationClientManager`.
void NearbyInternalsPushNotificationHandler::HandleAddPushNotificationClient(
    const base::Value::List& args) {
  push_notification::PushNotificationService* service =
      push_notification::PushNotificationServiceFactory::GetForBrowserContext(
          context_);
  CHECK(service);
}
