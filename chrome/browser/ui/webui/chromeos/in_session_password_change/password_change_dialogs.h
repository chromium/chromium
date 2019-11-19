// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_DIALOGS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_DIALOGS_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// A modal system dialog without any frame decorating it.
class BasePasswordDialog : public SystemWebDialogDelegate {
 protected:
  BasePasswordDialog(GURL url, gfx::Size desired_size);
  ~BasePasswordDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  ui::ModalType GetDialogModalType() const override;

 private:
  gfx::Size desired_size_;

  DISALLOW_COPY_AND_ASSIGN(BasePasswordDialog);
};

// System dialog wrapping chrome:://password-change
class PasswordChangeDialog : public BasePasswordDialog {
 public:
  static void Show();
  static void Dismiss();

 protected:
  PasswordChangeDialog();
  ~PasswordChangeDialog() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordChangeDialog);
};

// System dialog wrapping chrome://confirm-password-change
class ConfirmPasswordChangeDialog : public BasePasswordDialog {
 public:
  static void Show(const std::string& scraped_old_password,
                   const std::string& scraped_new_password,
                   bool show_spinner_initially);
  static void Dismiss();

  // How big does this dialog need to be to show these prompts:
  static gfx::Size GetSize(bool show_old_password_prompt,
                           bool show_new_password_prompt);

 protected:
  ConfirmPasswordChangeDialog(const std::string& scraped_old_password,
                              const std::string& scraped_new_password,
                              bool show_spinner_initially);
  ~ConfirmPasswordChangeDialog() override;

  // ui::WebDialogDelegate:
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;

 private:
  std::string scraped_old_password_;
  std::string scraped_new_password_;
  bool show_spinner_initially_ = false;

  DISALLOW_COPY_AND_ASSIGN(ConfirmPasswordChangeDialog);
};

// System dialog wrapping chrome://urgent-password-expiry-notification
class UrgentPasswordExpiryNotificationDialog : public BasePasswordDialog {
 public:
  static void Show();
  static void Dismiss();

 protected:
  UrgentPasswordExpiryNotificationDialog();
  ~UrgentPasswordExpiryNotificationDialog() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrgentPasswordExpiryNotificationDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_DIALOGS_H_
