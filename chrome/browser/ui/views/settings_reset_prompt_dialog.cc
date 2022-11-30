// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_reset_prompt_dialog.h"

#include <utility>

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowSettingsResetPrompt(
    Browser* browser,
    safe_browsing::SettingsResetPromptController* controller) {
  SettingsResetPromptDialog* dialog =
      new SettingsResetPromptDialog(browser, controller);
  // The dialog will delete itself, as implemented in
  // |DialogDelegateView::DeleteDelegate()|, when its widget is closed.
  dialog->Show();
}

}  // namespace chrome

SettingsResetPromptDialog::SettingsResetPromptDialog(
    Browser* browser,
    safe_browsing::SettingsResetPromptController* controller)
    : browser_(browser), controller_(controller) {
  DCHECK(browser_);
  DCHECK(controller_);

  SetShowIcon(false);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_SETTINGS_RESET_PROMPT_ACCEPT_BUTTON_LABEL));

  // There is at most one of {Accept(), Cancel(), Close()} will be run for
  // |controller_|. Each of them causes |controller_| deletion.
  SetAcceptCallback(base::BindOnce(
      [](SettingsResetPromptDialog* dialog) {
        std::exchange(dialog->controller_, nullptr)->Accept();
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](SettingsResetPromptDialog* dialog) {
        std::exchange(dialog->controller_, nullptr)->Cancel();
      },
      base::Unretained(this)));
  SetCloseCallback(base::BindOnce(
      [](SettingsResetPromptDialog* dialog) {
        std::exchange(dialog->controller_, nullptr)->Close();
      },
      base::Unretained(this)));

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::StyledLabel* const dialog_label =
      AddChildView(std::make_unique<views::StyledLabel>());
  dialog_label->SetText(controller_->GetMainText());
  dialog_label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  dialog_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  views::StyledLabel::RangeStyleInfo url_style;
  url_style.text_style = views::style::STYLE_EMPHASIZED_SECONDARY;
  dialog_label->AddStyleRange(controller_->GetMainTextUrlRange(), url_style);
}

SettingsResetPromptDialog::~SettingsResetPromptDialog() {
  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  if (controller_)
    controller_->Close();
}

void SettingsResetPromptDialog::Show() {
  DCHECK(controller_);
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_view->GetNativeWindow())
      ->Show();
  controller_->DialogShown();
}

std::u16string SettingsResetPromptDialog::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

BEGIN_METADATA(SettingsResetPromptDialog, views::DialogDelegateView)
END_METADATA
