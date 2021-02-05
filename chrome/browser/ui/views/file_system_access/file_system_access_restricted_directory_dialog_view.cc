// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_restricted_directory_dialog_view.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

using HandleType = content::FileSystemAccessPermissionContext::HandleType;

FileSystemAccessRestrictedDirectoryDialogView::
    ~FileSystemAccessRestrictedDirectoryDialogView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (!callback_.is_null())
    Accept();
}

// static
views::Widget* FileSystemAccessRestrictedDirectoryDialogView::ShowDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<void(SensitiveDirectoryResult)> callback,
    content::WebContents* web_contents) {
  auto delegate =
      base::WrapUnique(new FileSystemAccessRestrictedDirectoryDialogView(
          origin, path, handle_type, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

FileSystemAccessRestrictedDirectoryDialogView::
    FileSystemAccessRestrictedDirectoryDialogView(
        const url::Origin& origin,
        const base::FilePath& path,
        content::FileSystemAccessPermissionContext::HandleType handle_type,
        base::OnceCallback<void(SensitiveDirectoryResult)> callback)
    : handle_type_(handle_type), callback_(std::move(callback)) {
  SetTitle(handle_type_ == HandleType::kDirectory
               ? IDS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_TITLE
               : IDS_FILE_SYSTEM_ACCESS_RESTRICTED_FILE_TITLE);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     handle_type_ == HandleType::kDirectory
                         ? IDS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_BUTTON
                         : IDS_FILE_SYSTEM_ACCESS_RESTRICTED_FILE_BUTTON));

  auto run_callback = [](FileSystemAccessRestrictedDirectoryDialogView* dialog,
                         SensitiveDirectoryResult result) {
    std::move(dialog->callback_).Run(result);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   SensitiveDirectoryResult::kTryAgain));
  SetCancelCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   SensitiveDirectoryResult::kAbort));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  AddChildView(file_system_access_ui_helper::CreateOriginLabel(
      handle_type_ == HandleType::kDirectory
          ? IDS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_TEXT
          : IDS_FILE_SYSTEM_ACCESS_RESTRICTED_FILE_TEXT,
      origin, views::style::CONTEXT_DIALOG_BODY_TEXT, /*show_emphasis=*/true));
}

BEGIN_METADATA(FileSystemAccessRestrictedDirectoryDialogView,
               views::DialogDelegateView)
END_METADATA

void ShowFileSystemAccessRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<void(
        content::FileSystemAccessPermissionContext::SensitiveDirectoryResult)>
        callback,
    content::WebContents* web_contents) {
  FileSystemAccessRestrictedDirectoryDialogView::ShowDialog(
      origin, path, handle_type, std::move(callback), web_contents);
}
