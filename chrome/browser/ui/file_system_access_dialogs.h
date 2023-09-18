// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_DIALOGS_H_
#define CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_DIALOGS_H_

#include "base/functional/callback.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "content/public/browser/file_system_access_permission_context.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace permissions {
enum class PermissionAction;
}

namespace url {
class Origin;
}  // namespace url

// Displays a dialog to ask for write access to the given file or directory for
// the File System Access API.
void ShowFileSystemAccessPermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents);

// Displays a dialog to inform the user that the `path` they picked using the
// File System Access API is blocked by chrome. `callback` is called when the
// user has dismissed the dialog.
void ShowFileSystemAccessRestrictedDirectoryDialog(
    const url::Origin& origin,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback,
    content::WebContents* web_contents);

// Displays a dialog to explain to the user that the file at `path` has a
// dangerous extension and ask whether they still want to save the file.
// `callback` is called when the user has accepted or rejected the dialog.
void ShowFileSystemAccessDangerousFileDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback,
    content::WebContents* web_contents);

// Displays a dialog to restore permission for recently granted file or
// directory handles.
void ShowFileSystemAccessRestorePermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_DIALOGS_H_
