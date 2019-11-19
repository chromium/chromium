// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

class UrgentPasswordExpiryNotificationHandler
    : public content::WebUIMessageHandler {
 public:
  UrgentPasswordExpiryNotificationHandler();
  ~UrgentPasswordExpiryNotificationHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // User taps the button and agrees to change their password.
  void HandleContinue(const base::ListValue* params);

  // Need to update title to show new time remaining until password expiry.
  void HandleGetTitleText(const base::ListValue* params);

 private:
  base::WeakPtrFactory<UrgentPasswordExpiryNotificationHandler> weak_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(UrgentPasswordExpiryNotificationHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HANDLER_H_
