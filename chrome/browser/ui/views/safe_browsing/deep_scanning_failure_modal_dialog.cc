// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/deep_scanning_failure_modal_dialog.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_delegate.h"

namespace safe_browsing {

/*static*/
void DeepScanningFailureModalDialog::ShowForWebContents(
    content::WebContents* web_contents,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure open_now_callback) {
  constrained_window::ShowWebModalDialogViews(
      new DeepScanningFailureModalDialog(std::move(accept_callback),
                                         std::move(cancel_callback),
                                         std::move(open_now_callback)),
      web_contents);
}

DeepScanningFailureModalDialog::DeepScanningFailureModalDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure open_now_callback)
    : open_now_callback_(std::move(open_now_callback)) {
  SetTitle(IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_TITLE);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_ACCEPT_BUTTON));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_CANCEL_BUTTON));
  SetAcceptCallback(std::move(accept_callback));
  SetCancelCallback(std::move(cancel_callback));
  open_now_button_ = SetExtraView(std::make_unique<views::MdTextButton>(
      this, l10n_util::GetStringUTF16(
                IDS_DEEP_SCANNING_INFO_DIALOG_OPEN_NOW_BUTTON)));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Use a fixed maximum message width, so longer messages will wrap.
  const int kMaxMessageWidth = 400;
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kFixed, kMaxMessageWidth, false);

  // Add the message label.
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_MESSAGE),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SizeToFit(kMaxMessageWidth);
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(std::move(label));
}

DeepScanningFailureModalDialog::~DeepScanningFailureModalDialog() = default;

bool DeepScanningFailureModalDialog::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return (button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
}

bool DeepScanningFailureModalDialog::ShouldShowCloseButton() const {
  return false;
}

ui::ModalType DeepScanningFailureModalDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void DeepScanningFailureModalDialog::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (sender == open_now_button_) {
    std::move(open_now_callback_).Run();
    CancelDialog();
  }
}

}  // namespace safe_browsing
