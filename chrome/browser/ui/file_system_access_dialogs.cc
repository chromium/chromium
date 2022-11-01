// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access_dialogs.h"
#include "build/build_config.h"

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
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly dismissed.
  std::move(callback).Run(
      content::FileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
}

void ShowFileSystemAccessDangerousFileDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly dismissed.
  std::move(callback).Run(
      content::FileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
}

#endif  // !defined(TOOLKIT_VIEWS)
