// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_DIALOG_VIEW_H_

#include "chrome/browser/ui/browser.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace views {
class Widget;
}  // namespace views

// A dialog that informs the user that they can't give a website access to a
// specific folder.
class FileSystemAccessRestrictedDirectoryDialogView
    : public views::DialogDelegateView {
 public:
  METADATA_HEADER(FileSystemAccessRestrictedDirectoryDialogView);

  using SensitiveDirectoryResult =
      content::FileSystemAccessPermissionContext::SensitiveEntryResult;

  FileSystemAccessRestrictedDirectoryDialogView(
      const FileSystemAccessRestrictedDirectoryDialogView&) = delete;
  FileSystemAccessRestrictedDirectoryDialogView& operator=(
      const FileSystemAccessRestrictedDirectoryDialogView&) = delete;
  ~FileSystemAccessRestrictedDirectoryDialogView() override;

  // Creates and shows the dialog. `callback` is called when the dialog is
  // dismissed.
  static views::Widget* ShowDialog(
      const url::Origin& origin,
      const base::FilePath& path,
      content::FileSystemAccessPermissionContext::HandleType handle_type,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback,
      content::WebContents* web_contents);

 private:
  FileSystemAccessRestrictedDirectoryDialogView(
      Browser* browser,
      const url::Origin& origin,
      const base::FilePath& path,
      content::FileSystemAccessPermissionContext::HandleType handle_type,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback);

  const content::FileSystemAccessPermissionContext::HandleType handle_type_;
  base::OnceCallback<void(SensitiveDirectoryResult)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_DIALOG_VIEW_H_
