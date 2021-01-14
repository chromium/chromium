// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access_dialogs.h"

#include "components/permissions/permission_util.h"

#if !defined(TOOLKIT_VIEWS)
void ShowFileSystemAccessPermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly cancelled.
  std::move(callback).Run(permissions::PermissionAction::DISMISSED);
}

void ShowFileSystemAccessRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<void(
        content::FileSystemAccessPermissionContext::SensitiveDirectoryResult)>
        callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly dismissed.
  std::move(callback).Run(content::FileSystemAccessPermissionContext::
                              SensitiveDirectoryResult::kAbort);
}

#endif  // !defined(TOOLKIT_VIEWS)
