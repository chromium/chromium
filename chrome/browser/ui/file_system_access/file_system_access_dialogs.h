// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DIALOGS_H_
#define CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DIALOGS_H_

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

// Displays a dialog to restore permission for recently granted file or
// directory handles.
void ShowFileSystemAccessRestorePermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DIALOGS_H_
