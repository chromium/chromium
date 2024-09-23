// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_DIALOG_H_
#define CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_DIALOG_H_

#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace permissions {
enum class PermissionAction;
}  // namespace permissions

namespace ui {
class DialogModel;
}  // namespace ui

// Shows a dialog asking the user if they want to give write access to the file
// or directory identified by `request`. `callback` will be called with the
// users choice, either GRANTED or DISMISSED.
void ShowFileSystemAccessPermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents);

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessPermissionDialogForTesting(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback);

#endif  // CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_DIALOG_H_
