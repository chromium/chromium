// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace permissions {
enum class PermissionAction;
}

namespace views {
class Widget;
}  // namespace views

// Dialog that asks for write access to the given file or directory for the
// native file system API.
class NativeFileSystemPermissionView : public views::DialogDelegateView {
 public:
  using Request = NativeFileSystemPermissionRequestManager::RequestData;

  ~NativeFileSystemPermissionView() override;

  // Shows a dialog asking the user if they want to give write access to the
  // file or directory identified by |path| to the given |origin|.
  // |callback| will be called with the users choice, either GRANTED or
  // DISMISSED.
  static views::Widget* ShowDialog(
      const Request& request,
      base::OnceCallback<void(permissions::PermissionAction result)> callback,
      content::WebContents* web_contents);

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;

 private:
  NativeFileSystemPermissionView(
      const Request& request,
      base::OnceCallback<void(permissions::PermissionAction result)> callback);

  const Request request_;
  base::OnceCallback<void(permissions::PermissionAction result)> callback_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemPermissionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_VIEW_H_
