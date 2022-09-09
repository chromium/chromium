// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_dangerous_file_dialog_view.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

FileSystemAccessDangerousFileDialogView::
    ~FileSystemAccessDangerousFileDialogView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (callback_)
    Close();
  DCHECK(!callback_);
}

// static
views::Widget* FileSystemAccessDangerousFileDialogView::ShowDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(DangerousFileResult)> callback,
    content::WebContents* web_contents) {
  auto delegate = base::WrapUnique(new FileSystemAccessDangerousFileDialogView(
      origin, path, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

FileSystemAccessDangerousFileDialogView::
    FileSystemAccessDangerousFileDialogView(
        const url::Origin& origin,
        const base::FilePath& path,
        base::OnceCallback<void(DangerousFileResult)> callback)
    : callback_(std::move(callback)) {
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_TITLE,
      file_system_access_ui_helper::GetPathForDisplay(path)));
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_SAVE));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DONT_SAVE));
  // Ensure the default is to not save the dangerous file.
  SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);

  auto run_callback = [](FileSystemAccessDangerousFileDialogView* dialog,
                         DangerousFileResult result) {
    std::move(dialog->callback_).Run(result);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   DangerousFileResult::kAllowed));
  SetCancelCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   DangerousFileResult::kAbort));
  SetCloseCallback(base::BindOnce(run_callback, base::Unretained(this),
                                  DangerousFileResult::kAbort));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  AddChildView(file_system_access_ui_helper::CreateOriginLabel(
      IDS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_TEXT, origin,
      views::style::CONTEXT_DIALOG_BODY_TEXT, /*show_emphasis=*/true));
}

BEGIN_METADATA(FileSystemAccessDangerousFileDialogView,
               views::DialogDelegateView)
END_METADATA

void ShowFileSystemAccessDangerousFileDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(
        FileSystemAccessDangerousFileDialogView::DangerousFileResult)> callback,
    content::WebContents* web_contents) {
  FileSystemAccessDangerousFileDialogView::ShowDialog(
      origin, path, std::move(callback), web_contents);
}
