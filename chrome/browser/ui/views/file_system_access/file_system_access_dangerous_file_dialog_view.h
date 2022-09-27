// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DIALOG_VIEW_H_

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

// A dialog that asks the user whether they want to save a file with a dangerous
// extension.
class FileSystemAccessDangerousFileDialogView
    : public views::DialogDelegateView {
 public:
  METADATA_HEADER(FileSystemAccessDangerousFileDialogView);

  using DangerousFileResult =
      content::FileSystemAccessPermissionContext::SensitiveEntryResult;

  FileSystemAccessDangerousFileDialogView(
      const FileSystemAccessDangerousFileDialogView&) = delete;
  FileSystemAccessDangerousFileDialogView& operator=(
      const FileSystemAccessDangerousFileDialogView&) = delete;
  ~FileSystemAccessDangerousFileDialogView() override;

  // Creates and shows the dialog. `callback` is called when the dialog is
  // dismissed.
  static views::Widget* ShowDialog(
      const url::Origin& origin,
      const base::FilePath& path,
      base::OnceCallback<void(DangerousFileResult)> callback,
      content::WebContents* web_contents);

 private:
  FileSystemAccessDangerousFileDialogView(
      Browser* browser,
      const url::Origin& origin,
      const base::FilePath& path,
      base::OnceCallback<void(DangerousFileResult)> callback);

  base::OnceCallback<void(DangerousFileResult)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DIALOG_VIEW_H_
