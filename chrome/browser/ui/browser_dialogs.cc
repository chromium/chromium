// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/permissions/permission_util.h"

namespace chrome {

void RecordDialogCreation(DialogIdentifier identifier) {
  UMA_HISTOGRAM_ENUMERATION("Dialog.Creation", identifier,
                            DialogIdentifier::MAX_VALUE);
}

}  // namespace chrome

#if !defined(TOOLKIT_VIEWS)
// There's no dialog version of this available outside views, run callback as if
// the dialog was instantly accepted.
void ShowFolderUploadConfirmationDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents) {
  std::move(callback).Run(selected_files);
}

void ShowNativeFileSystemPermissionDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly cancelled.
  std::move(callback).Run(PermissionAction::DISMISSED);
}

void ShowNativeFileSystemRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(
        content::NativeFileSystemPermissionContext::SensitiveDirectoryResult)>
        callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly dismissed.
  std::move(callback).Run(content::NativeFileSystemPermissionContext::
                              SensitiveDirectoryResult::kAbort);
}

void ShowNativeFileSystemDirectoryAccessConfirmationDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly dismissed.
  std::move(callback).Run(PermissionAction::DISMISSED);
}

#endif  // !defined(TOOLKIT_VIEWS)
