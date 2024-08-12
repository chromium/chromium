// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_DIALOG_H_
#define CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_DIALOG_H_

#include "content/public/browser/file_system_access_permission_context.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class DialogModel;
}  // namespace ui

namespace url {
class Origin;
}  // namespace url

// A dialog that informs the user that they can't give a website access to a
// specific folder. `callback` is called when the dialog is dismissed.
void ShowFileSystemAccessRestrictedDirectoryDialog(
    const url::Origin& origin,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback,
    content::WebContents* web_contents);

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessRestrictedDirectoryDialogForTesting(
    const url::Origin& origin,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback);

#endif  // CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTRICTED_DIRECTORY_DIALOG_H_
