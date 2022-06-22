// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NOTIFICATION_TESTER_NOTIFICATION_TESTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NOTIFICATION_TESTER_NOTIFICATION_TESTER_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

// WebUI message handler for chrome://notification-tester from the front-end to
// the message center.
class NotificationTesterHandler : public content::WebUIMessageHandler {
 public:
  NotificationTesterHandler();
  NotificationTesterHandler(const NotificationTesterHandler&) = delete;
  NotificationTesterHandler& operator=(const NotificationTesterHandler&) =
      delete;
  ~NotificationTesterHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // MessageHandler callback that fires when the MessageHandler receives a
  // message to generate a notification from the front-end.
  void HandleGenerateNotificationForm(const base::Value::List& args);

  // Generates a notification via the message center with the given title and
  // body.
  void GenerateNotification(const std::u16string& title,
                            const std::u16string& message);
};

}  // namespace chromeos

#endif
