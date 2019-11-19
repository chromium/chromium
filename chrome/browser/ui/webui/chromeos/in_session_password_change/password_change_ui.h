// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// For chrome:://password-change
class PasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit PasswordChangeUI(content::WebUI* web_ui);
  ~PasswordChangeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordChangeUI);
};

// For chrome:://confirm-password-change
class ConfirmPasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit ConfirmPasswordChangeUI(content::WebUI* web_ui);
  ~ConfirmPasswordChangeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfirmPasswordChangeUI);
};

// For chrome:://urgent-password-expiry-notification
class UrgentPasswordExpiryNotificationUI : public ui::WebDialogUI {
 public:
  explicit UrgentPasswordExpiryNotificationUI(content::WebUI* web_ui);
  ~UrgentPasswordExpiryNotificationUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrgentPasswordExpiryNotificationUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_
