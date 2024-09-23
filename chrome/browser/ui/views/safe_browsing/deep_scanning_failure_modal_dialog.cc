// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/deep_scanning_failure_modal_dialog.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/window/dialog_delegate.h"

namespace safe_browsing {

/*static*/
void DeepScanningFailureModalDialog::ShowForWebContents(
    content::WebContents* web_contents,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure close_callback,
    base::OnceClosure open_now_callback) {
  constrained_window::ShowWebModalDialogViews(
      new DeepScanningFailureModalDialog(
          std::move(accept_callback), std::move(cancel_callback),
          std::move(close_callback), std::move(open_now_callback)),
      web_contents);
}

DeepScanningFailureModalDialog::DeepScanningFailureModalDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure close_callback,
    base::OnceClosure open_now_callback)
    : open_now_callback_(std::move(open_now_callback)) {
  SetModalType(ui::mojom::ModalType::kChild);
  SetTitle(IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_TITLE);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_ACCEPT_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_CANCEL_BUTTON));
  SetAcceptCallback(std::move(accept_callback));
  SetCancelCallback(std::move(cancel_callback));
  SetCloseCallback(std::move(close_callback));
  SetExtraView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](DeepScanningFailureModalDialog* dialog) {
            std::move(dialog->open_now_callback_).Run();
            dialog->CancelDialog();
          },
          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_DEEP_SCANNING_INFO_DIALOG_OPEN_NOW_BUTTON)));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetUseDefaultFillLayout(true);

  // Add the message label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_TIMED_OUT_DIALOG_MESSAGE),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  constexpr int kMaxMessageWidth = 400;
  label->SetMaximumWidth(kMaxMessageWidth);
}

DeepScanningFailureModalDialog::~DeepScanningFailureModalDialog() = default;

bool DeepScanningFailureModalDialog::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return (button == ui::mojom::DialogButton::kOk ||
          button == ui::mojom::DialogButton::kCancel);
}

bool DeepScanningFailureModalDialog::ShouldShowCloseButton() const {
  return false;
}

BEGIN_METADATA(DeepScanningFailureModalDialog)
END_METADATA

}  // namespace safe_browsing
