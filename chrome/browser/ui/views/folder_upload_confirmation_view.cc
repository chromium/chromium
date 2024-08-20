// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/folder_upload_confirmation_view.h"

#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

FolderUploadConfirmationView::FolderUploadConfirmationView(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files)
    : callback_(std::move(callback)),
      selected_files_(std::move(selected_files)) {
  SetTitle(l10n_util::GetPluralStringFUTF16(
      IDS_CONFIRM_FILE_UPLOAD_TITLE,
      base::saturated_cast<int>(selected_files_.size())));

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_CONFIRM_FILE_UPLOAD_OK_BUTTON));

  SetAcceptCallback(base::BindOnce(
      [](FolderUploadConfirmationView* dialog) {
        std::move(dialog->callback_).Run(dialog->selected_files_);
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](FolderUploadConfirmationView* dialog) {
        std::move(dialog->callback_).Run({});
      },
      base::Unretained(this)));
  SetCloseCallback(base::BindOnce(
      [](FolderUploadConfirmationView* dialog) {
        std::move(dialog->callback_).Run({});
      },
      base::Unretained(this)));

  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetUseDefaultFillLayout(true);
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_CONFIRM_FILE_UPLOAD_TEXT,
                                 path.BaseName().LossyDisplayName()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(label));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
}

FolderUploadConfirmationView::~FolderUploadConfirmationView() {
  // Make sure the dialog ends up calling the callback no matter what as
  // FileSelectHelper keeps itself alive until it sends the result.
  if (!callback_.is_null())
    Cancel();
}

views::Widget* FolderUploadConfirmationView::ShowDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents) {
  auto delegate = std::make_unique<FolderUploadConfirmationView>(
      path, std::move(callback), std::move(selected_files));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

views::View* FolderUploadConfirmationView::GetInitiallyFocusedView() {
  return GetCancelButton();
}

BEGIN_METADATA(FolderUploadConfirmationView)
END_METADATA

void ShowFolderUploadConfirmationDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents) {
  FolderUploadConfirmationView::ShowDialog(
      path, std::move(callback), std::move(selected_files), web_contents);
}
