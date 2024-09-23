// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_dialogs.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/confirm_password_change_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

PasswordChangeDialog* g_dialog = nullptr;

ConfirmPasswordChangeDialog* g_confirm_dialog = nullptr;

UrgentPasswordExpiryNotificationDialog* g_notification_dialog = nullptr;

constexpr gfx::Size kPasswordChangeSize(768, 640);

constexpr gfx::Size kUrgentPasswordExpiryNotificationSize = kPasswordChangeSize;

// The size of the confirm password change UI depends on which passwords were
// scraped and which ones we need to prompt for:
constexpr int kConfirmPasswordsWidth = 520;
constexpr int kConfirmOldPasswordHeight = 230;
constexpr int kConfirmNewPasswordHeight = 310;
constexpr int kConfirmBothPasswordsHeight = 380;

// Given a desired size, returns the same size if it fits on screen,
// or the closest possible size that will fit on the screen.
gfx::Size FitSizeToDisplay(const gfx::Size& desired) {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  return gfx::Size(std::min(desired.width(), display_size.width()),
                   std::min(desired.height(), display_size.height()));
}

}  // namespace

BasePasswordDialog::BasePasswordDialog(GURL url, gfx::Size desired_size)
    : SystemWebDialogDelegate(url, /*title=*/std::u16string()),
      desired_size_(desired_size) {}

BasePasswordDialog::~BasePasswordDialog() {}

void BasePasswordDialog::GetDialogSize(gfx::Size* size) const {
  *size = FitSizeToDisplay(desired_size_);
}

void BasePasswordDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

ui::mojom::ModalType BasePasswordDialog::GetDialogModalType() const {
  return ui::mojom::ModalType::kSystem;
}

// static
void PasswordChangeDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = new PasswordChangeDialog();
  g_dialog->ShowSystemDialog();
}

// static
void PasswordChangeDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog)
    g_dialog->Close();
}

PasswordChangeDialog::PasswordChangeDialog()
    : BasePasswordDialog(GURL(chrome::kChromeUIPasswordChangeUrl),
                         kPasswordChangeSize) {}

PasswordChangeDialog::~PasswordChangeDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

// static
void ConfirmPasswordChangeDialog::Show(const std::string& scraped_old_password,
                                       const std::string& scraped_new_password,
                                       bool show_spinner_initially) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_confirm_dialog) {
    g_confirm_dialog->Focus();
    return;
  }
  g_confirm_dialog = new ConfirmPasswordChangeDialog(
      scraped_old_password, scraped_new_password, show_spinner_initially);
  g_confirm_dialog->ShowSystemDialog();
}

// static
void ConfirmPasswordChangeDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_confirm_dialog)
    g_confirm_dialog->Close();
}

ConfirmPasswordChangeDialog::ConfirmPasswordChangeDialog(
    const std::string& scraped_old_password,
    const std::string& scraped_new_password,
    bool show_spinner_initially)
    : BasePasswordDialog(
          GURL(chrome::kChromeUIConfirmPasswordChangeUrl),
          GetSize(scraped_old_password.empty(), scraped_new_password.empty())),
      scraped_old_password_(scraped_old_password),
      scraped_new_password_(scraped_new_password),
      show_spinner_initially_(show_spinner_initially) {}

ConfirmPasswordChangeDialog::~ConfirmPasswordChangeDialog() {
  DCHECK_EQ(this, g_confirm_dialog);
  g_confirm_dialog = nullptr;
}

// static
gfx::Size ConfirmPasswordChangeDialog::GetSize(
    const bool show_old_password_prompt,
    const bool show_new_password_prompt) {
  const int desired_width = kConfirmPasswordsWidth;
  if (show_old_password_prompt && show_new_password_prompt) {
    return gfx::Size(desired_width, kConfirmBothPasswordsHeight);
  }
  if (show_new_password_prompt) {
    return gfx::Size(desired_width, kConfirmNewPasswordHeight);
  }
  // Use the same size for these two cases:
  // 1) We scraped new password, but not old, so we need to prompt for that.
  // 2) We scraped both passwords, so we don't need to prompt for anything.

  // In case 2, we need to show a spinner. That spinner could be any size, so
  // we size it the same as in case 1, because there is a chance that the
  // scraped password will be wrong and so we will need to show the old password
  // prompt. So it looks best if the dialog is already the right size.
  return gfx::Size(desired_width, kConfirmOldPasswordHeight);
}

void ConfirmPasswordChangeDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) {
  handlers->push_back(new ConfirmPasswordChangeHandler(
      scraped_old_password_, scraped_new_password_, show_spinner_initially_));
}

// static
void UrgentPasswordExpiryNotificationDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_notification_dialog) {
    g_notification_dialog->Focus();
    return;
  }
  g_notification_dialog = new UrgentPasswordExpiryNotificationDialog();
  g_notification_dialog->ShowSystemDialog();
}

// static
void UrgentPasswordExpiryNotificationDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_notification_dialog)
    g_notification_dialog->Close();
}

UrgentPasswordExpiryNotificationDialog::UrgentPasswordExpiryNotificationDialog()
    : BasePasswordDialog(
          GURL(chrome::kChromeUIUrgentPasswordExpiryNotificationUrl),
          kUrgentPasswordExpiryNotificationSize) {}

UrgentPasswordExpiryNotificationDialog::
    ~UrgentPasswordExpiryNotificationDialog() {
  DCHECK_EQ(this, g_notification_dialog);
  g_notification_dialog = nullptr;
}

}  // namespace ash
