// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/window/dialog_delegate.h"

enum class PermissionAction;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace views {
class Widget;
}  // namespace views

// Dialog that asks for write access to the given file or directory for the
// native file system API.
class NativeFileSystemPermissionView : public views::DialogDelegateView {
 public:
  ~NativeFileSystemPermissionView() override;

  // Shows a dialog asking the user if they want to give write access to the
  // file or directory identified by |path| to the given |origin|.
  // |callback| will be called with the users choice, either GRANTED or
  // DISMISSED.
  static views::Widget* ShowDialog(
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory,
      base::OnceCallback<void(PermissionAction result)> callback,
      content::WebContents* web_contents);

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  views::View* GetInitiallyFocusedView() override;

 private:
  NativeFileSystemPermissionView(
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory,
      base::OnceCallback<void(PermissionAction result)> callback);

  const base::FilePath path_;
  base::OnceCallback<void(PermissionAction result)> callback_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemPermissionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_VIEW_H_
