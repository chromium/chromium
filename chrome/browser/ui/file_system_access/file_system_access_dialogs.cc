// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_dialogs.h"

#include "build/build_config.h"
#include "components/permissions/permission_util.h"

#if !defined(TOOLKIT_VIEWS)
void ShowFileSystemAccessRestorePermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly cancelled.
  // TODO(crbug.com/40234828) / TODO(crbug.com/40101963): Migrate
  // file_system_access_restore_permission_bubble_view.cc to use ui::DialogModel
  // and remove dependencies on views.
  std::move(callback).Run(permissions::PermissionAction::DISMISSED);
}

#endif  // !defined(TOOLKIT_VIEWS)
