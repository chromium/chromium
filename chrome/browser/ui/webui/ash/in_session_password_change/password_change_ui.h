// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class PasswordChangeUI;
class ConfirmPasswordChangeUI;
class UrgentPasswordExpiryNotificationUI;

// WebUIConfig for chrome://password-change
class PasswordChangeUIConfig
    : public content::DefaultWebUIConfig<PasswordChangeUI> {
 public:
  PasswordChangeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPasswordChangeHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// For chrome:://password-change
class PasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit PasswordChangeUI(content::WebUI* web_ui);

  PasswordChangeUI(const PasswordChangeUI&) = delete;
  PasswordChangeUI& operator=(const PasswordChangeUI&) = delete;

  ~PasswordChangeUI() override;
};

// WebUIConfig for chrome://confirm-password-change
class ConfirmPasswordChangeUIConfig
    : public content::DefaultWebUIConfig<ConfirmPasswordChangeUI> {
 public:
  ConfirmPasswordChangeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIConfirmPasswordChangeHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// For chrome:://confirm-password-change
class ConfirmPasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit ConfirmPasswordChangeUI(content::WebUI* web_ui);

  ConfirmPasswordChangeUI(const ConfirmPasswordChangeUI&) = delete;
  ConfirmPasswordChangeUI& operator=(const ConfirmPasswordChangeUI&) = delete;

  ~ConfirmPasswordChangeUI() override;
};

// WebUIConfig for chrome:://urgent-password-expiry-notification
class UrgentPasswordExpiryNotificationUIConfig
    : public content::DefaultWebUIConfig<UrgentPasswordExpiryNotificationUI> {
 public:
  UrgentPasswordExpiryNotificationUIConfig()
      : DefaultWebUIConfig(
            content::kChromeUIScheme,
            chrome::kChromeUIUrgentPasswordExpiryNotificationHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// For chrome:://urgent-password-expiry-notification
class UrgentPasswordExpiryNotificationUI : public ui::WebDialogUI {
 public:
  explicit UrgentPasswordExpiryNotificationUI(content::WebUI* web_ui);

  UrgentPasswordExpiryNotificationUI(
      const UrgentPasswordExpiryNotificationUI&) = delete;
  UrgentPasswordExpiryNotificationUI& operator=(
      const UrgentPasswordExpiryNotificationUI&) = delete;

  ~UrgentPasswordExpiryNotificationUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_
