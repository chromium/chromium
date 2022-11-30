// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_cleaner_reboot_dialog_win.h"

#include <string>

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

  set_draggable(true);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_CHROME_CLEANUP_REBOOT_PROMPT_RESTART_BUTTON_LABEL));
  SetModalType(ui::MODAL_TYPE_NONE);

  using Controller = safe_browsing::ChromeCleanerRebootDialogController;
  using ControllerClosureFn = void (Controller::*)(void);
  auto close_callback = [](Controller** controller, ControllerClosureFn fn) {
    // This lambda gets bound later to form callbacks for the dialog's close
    // methods (Accept, Cancel, Close). At most one of these three callbacks may
    // be invoked, so it swaps this instance's controller pointer with nullptr,
    // which inhibits a second callback to the controller in
    // ~ChromeCleanerRebootDialog.
    (std::exchange(*controller, nullptr)->*(fn))();
  };

  SetAcceptCallback(base::BindOnce(close_callback,
                                   base::Unretained(&dialog_controller_),
                                   &Controller::Accept));
  SetCancelCallback(base::BindOnce(close_callback,
                                   base::Unretained(&dialog_controller_),
                                   &Controller::Cancel));
  SetCloseCallback(base::BindOnce(close_callback,
                                  base::Unretained(&dialog_controller_),
                                  &Controller::Close));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
}

ChromeCleanerRebootDialog::~ChromeCleanerRebootDialog() {
  // If the controller is still non-null, none of the dialog's closure methods
  // have run - see this class's constructor. In that case, notify the
  // controller that this dialog is going away.
  if (dialog_controller_)
    std::exchange(dialog_controller_, nullptr)->Close();
}

void ChromeCleanerRebootDialog::Show(Browser* browser) {
  DCHECK(browser);
  DCHECK(dialog_controller_);

  views::Widget* widget =
      CreateDialogWidget(this, nullptr, browser->window()->GetNativeWindow());
  widget->SetBounds(GetDialogBounds(browser));
  widget->Show();
}

// WidgetDelegate overrides.

std::u16string ChromeCleanerRebootDialog::GetWindowTitle() const {
  DCHECK(dialog_controller_);
  return l10n_util::GetStringUTF16(IDS_CHROME_CLEANUP_REBOOT_PROMPT_TITLE);
}

views::View* ChromeCleanerRebootDialog::GetInitiallyFocusedView() {
  // Set focus away from the Restart/OK button to prevent accidental prompt
  // acceptance if the user is typing as the dialog appears.
  return GetCancelButton();
}

gfx::Rect ChromeCleanerRebootDialog::GetDialogBounds(Browser* browser) const {
  gfx::Rect browser_bounds = browser->window()->GetBounds();
  return gfx::Rect(
      browser_bounds.x() + (browser_bounds.width() - kDialogWidth) / 2,
      browser_bounds.y() + kDialogYOffset, kDialogWidth, kDialogHeight);
}

BEGIN_METADATA(ChromeCleanerRebootDialog, views::DialogDelegateView)
END_METADATA
