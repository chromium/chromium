// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_VIEW_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "ui/views/metadata/metadata_header_macros.h"
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
// File System Access API.
class FileSystemAccessPermissionView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(FileSystemAccessPermissionView);

  using Request = FileSystemAccessPermissionRequestManager::RequestData;

  FileSystemAccessPermissionView(const FileSystemAccessPermissionView&) =
      delete;
  FileSystemAccessPermissionView& operator=(
      const FileSystemAccessPermissionView&) = delete;
  ~FileSystemAccessPermissionView() override;

  // Shows a dialog asking the user if they want to give write access to the
  // file or directory identified by |path| to the given |origin|.
  // |callback| will be called with the users choice, either GRANTED or
  // DISMISSED.
  static views::Widget* ShowDialog(
      const Request& request,
      base::OnceCallback<void(permissions::PermissionAction result)> callback,
      content::WebContents* web_contents);

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;

 private:
  FileSystemAccessPermissionView(
      const Request& request,
      base::OnceCallback<void(permissions::PermissionAction result)> callback);

  const Request request_;
  base::OnceCallback<void(permissions::PermissionAction result)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_VIEW_H_
