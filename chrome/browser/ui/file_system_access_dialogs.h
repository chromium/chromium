// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_DIALOGS_H_
#define CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_DIALOGS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
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

// Displays a dialog to inform the user that the |path| they picked using the
// File System Access API is blocked by chrome. |is_directory| is true if the
// user was selecting a directory, otherwise the user was selecting files within
// a directory. |callback| is called when the user has dismissed the dialog.
void ShowFileSystemAccessRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<void(
        content::FileSystemAccessPermissionContext::SensitiveDirectoryResult)>
        callback,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_DIALOGS_H_
