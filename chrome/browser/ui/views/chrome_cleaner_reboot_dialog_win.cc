// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_cleaner_reboot_dialog_win.h"

#include "base/strings/string16.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowChromeCleanerRebootPrompt(
    Browser* browser,
    safe_browsing::ChromeCleanerRebootDialogController* dialog_controller) {
  DCHECK(browser);
  DCHECK(dialog_controller);

  ChromeCleanerRebootDialog* dialog =
      new ChromeCleanerRebootDialog(dialog_controller);
  dialog->Show(browser);
}

}  // namespace chrome

namespace {
constexpr int kDialogWidth = 448;
constexpr int kDialogHeight = 116;
constexpr int kDialogYOffset = 72;
}  // namespace

ChromeCleanerRebootDialog::ChromeCleanerRebootDialog(
    safe_browsing::ChromeCleanerRebootDialogController* dialog_controller)
    : dialog_controller_(dialog_controller) {
  DCHECK(dialog_controller_);

  DialogDelegate::set_draggable(true);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_CHROME_CLEANUP_REBOOT_PROMPT_RESTART_BUTTON_LABEL));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
}

ChromeCleanerRebootDialog::~ChromeCleanerRebootDialog() {
  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  if (dialog_controller_) {
    HandleDialogInteraction(DialogInteractionResult::kClosedOnDestruction);
  }
}

void ChromeCleanerRebootDialog::Show(Browser* browser) {
  DCHECK(browser);
  DCHECK(dialog_controller_);

  views::Widget* widget = DialogDelegate::CreateDialogWidget(
      this, nullptr, browser->window()->GetNativeWindow());
  widget->SetBounds(GetDialogBounds(browser));
  widget->Show();
}

// WidgetDelegate overrides.

ui::ModalType ChromeCleanerRebootDialog::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

base::string16 ChromeCleanerRebootDialog::GetWindowTitle() const {
  DCHECK(dialog_controller_);
  return l10n_util::GetStringUTF16(IDS_CHROME_CLEANUP_REBOOT_PROMPT_TITLE);
}

views::View* ChromeCleanerRebootDialog::GetInitiallyFocusedView() {
  // Set focus away from the Restart/OK button to prevent accidental prompt
  // acceptance if the user is typing as the dialog appears.
  return GetCancelButton();
}

// DialogDelegate overrides.
bool ChromeCleanerRebootDialog::Accept() {
  HandleDialogInteraction(DialogInteractionResult::kAccept);
  return true;
}

bool ChromeCleanerRebootDialog::Cancel() {
  HandleDialogInteraction(DialogInteractionResult::kCancel);
  return true;
}

bool ChromeCleanerRebootDialog::Close() {
  HandleDialogInteraction(DialogInteractionResult::kClose);
  return true;
}

void ChromeCleanerRebootDialog::HandleDialogInteraction(
    DialogInteractionResult result) {
  if (!dialog_controller_)
    return;

  switch (result) {
    case DialogInteractionResult::kAccept:
      dialog_controller_->Accept();
      break;
    case DialogInteractionResult::kCancel:
      dialog_controller_->Cancel();
      break;
    case DialogInteractionResult::kClose:
    case DialogInteractionResult::kClosedOnDestruction:
      // Fallthrough.
      dialog_controller_->Close();
      break;
  }
  dialog_controller_ = nullptr;
}

gfx::Rect ChromeCleanerRebootDialog::GetDialogBounds(Browser* browser) const {
  gfx::Rect browser_bounds = browser->window()->GetBounds();
  return gfx::Rect(
      browser_bounds.x() + (browser_bounds.width() - kDialogWidth) / 2,
      browser_bounds.y() + kDialogYOffset, kDialogWidth, kDialogHeight);
}
