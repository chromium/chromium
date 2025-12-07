// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DIALOG_H_
#define CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DIALOG_H_

#include "content/public/browser/file_system_access_permission_context.h"

namespace content {
struct PathInfo;
class WebContents;
}  // namespace content

namespace ui {
class DialogModel;
}  // namespace ui

namespace url {
class Origin;
}  // namespace url

// A dialog that asks the user whether they want to save a file with a dangerous
// extension. `callback` is called when the dialog is dismissed.
void ShowFileSystemAccessDangerousFileDialog(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback,
    content::WebContents* web_contents);

std::unique_ptr<ui::DialogModel>
CreateFileSystemAccessDangerousFileDialogForTesting(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    base::OnceCallback<
        void(content::FileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback);

#endif  // CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DANGEROUS_FILE_DIALOG_H_
