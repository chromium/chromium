// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_NOTIFICATIONS_NOTIFICATIONS_INTERNALS_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_NOTIFICATIONS_NOTIFICATIONS_INTERNALS_UI_MESSAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace notifications {
class NotificationScheduleService;
}  // namespace notifications

class Profile;

// Routes html events to native notification systems in
// chrome://internals/notifications.
class NotificationsInternalsUIMessageHandler
    : public content::WebUIMessageHandler {
 public:
  explicit NotificationsInternalsUIMessageHandler(Profile* profile);
  ~NotificationsInternalsUIMessageHandler() override;
  NotificationsInternalsUIMessageHandler(
      const NotificationsInternalsUIMessageHandler& other) = delete;
  NotificationsInternalsUIMessageHandler& operator=(
      const NotificationsInternalsUIMessageHandler& other) = delete;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleScheduleNotification(const base::Value::List& args);

  raw_ptr<notifications::NotificationScheduleService> schedule_service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_NOTIFICATIONS_NOTIFICATIONS_INTERNALS_UI_MESSAGE_HANDLER_H_
