// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

FolderUploadConfirmationView::FolderUploadConfirmationView(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files)
    : callback_(std::move(callback)),
      selected_files_(std::move(selected_files)) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
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

  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_CONFIRM_FILE_UPLOAD_TEXT,
                                 path.BaseName().LossyDisplayName()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label.release());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
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

base::string16 FolderUploadConfirmationView::GetWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_CONFIRM_FILE_UPLOAD_TITLE,
      base::saturated_cast<int>(selected_files_.size()));
}

views::View* FolderUploadConfirmationView::GetInitiallyFocusedView() {
  return GetCancelButton();
}

bool FolderUploadConfirmationView::ShouldShowCloseButton() const {
  return false;
}

gfx::Size FolderUploadConfirmationView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType FolderUploadConfirmationView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void ShowFolderUploadConfirmationDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents) {
  FolderUploadConfirmationView::ShowDialog(
      path, std::move(callback), std::move(selected_files), web_contents);
}
