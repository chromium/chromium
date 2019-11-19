// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_DIALOG_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/native_file_system_permission_context.h"
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
class NativeFileSystemRestrictedDirectoryDialogView
    : public views::DialogDelegateView {
 public:
  using SensitiveDirectoryResult =
      content::NativeFileSystemPermissionContext::SensitiveDirectoryResult;

  ~NativeFileSystemRestrictedDirectoryDialogView() override;

  // Creates and shows the dialog. The |callback| is called when the dialog is
  // dismissed.
  static views::Widget* ShowDialog(
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback,
      content::WebContents* web_contents);

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override;
  int GetDialogButtons() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;

 private:
  NativeFileSystemRestrictedDirectoryDialogView(
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback);

  const bool is_directory_;
  base::OnceCallback<void(SensitiveDirectoryResult)> callback_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemRestrictedDirectoryDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_DIALOG_VIEW_H_
