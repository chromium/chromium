// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_CONFIRMATION_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/permissions/permission_util.h"
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

// A dialog similar to FolderUploadConfirmationView that confirms that the user
// intended to give access to the specific folder.
// This is also a security measure against sites that trick a user into pressing
// enter, which would instantly confirm the OS folder picker and share the
// default folder selection without explicit user approval.
class NativeFileSystemDirectoryAccessConfirmationView
    : public views::DialogDelegateView {
 public:
  ~NativeFileSystemDirectoryAccessConfirmationView() override;

  static views::Widget* ShowDialog(
      const url::Origin& origin,
      const base::FilePath& path,
      base::OnceCallback<void(PermissionAction result)> callback,
      content::WebContents* web_contents);

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  // It's really important that this dialog *does not* accept by default /
  // when a user presses enter without looking as we're looking for explicit
  // approval to share this directory with the site.
  views::View* GetInitiallyFocusedView() override;

 private:
  NativeFileSystemDirectoryAccessConfirmationView(
      const url::Origin& origin,
      const base::FilePath& path,
      base::OnceCallback<void(PermissionAction result)> callback);

  base::OnceCallback<void(PermissionAction result)> callback_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemDirectoryAccessConfirmationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_CONFIRMATION_VIEW_H_
