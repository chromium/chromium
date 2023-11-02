// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

class UrgentPasswordExpiryNotificationHandler
    : public content::WebUIMessageHandler {
 public:
  UrgentPasswordExpiryNotificationHandler();

  UrgentPasswordExpiryNotificationHandler(
      const UrgentPasswordExpiryNotificationHandler&) = delete;
  UrgentPasswordExpiryNotificationHandler& operator=(
      const UrgentPasswordExpiryNotificationHandler&) = delete;

  ~UrgentPasswordExpiryNotificationHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // User taps the button and agrees to change their password.
  void HandleContinue(const base::Value::List& params);

  // Need to update title to show new time remaining until password expiry.
  void HandleGetTitleText(const base::Value::List& params);

 private:
  base::WeakPtrFactory<UrgentPasswordExpiryNotificationHandler> weak_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HANDLER_H_
