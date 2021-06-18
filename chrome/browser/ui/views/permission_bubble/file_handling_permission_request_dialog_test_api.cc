// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/file_handling_permission_request_dialog_test_api.h"

#include "chrome/browser/ui/views/permission_bubble/file_handling_permission_request_dialog.h"
#include "ui/views/controls/button/checkbox.h"

namespace web_app {

bool FileHandlingPermissionRequestDialogTestApi::IsShowing() {
  return !!FileHandlingPermissionRequestDialog::GetInstanceForTesting();
}

void FileHandlingPermissionRequestDialogTestApi::Resolve(bool checked,
                                                         bool accept) {
  auto* dialog = FileHandlingPermissionRequestDialog::GetInstanceForTesting();
  dialog->checkbox_->SetChecked(checked);
  if (accept)
    dialog->OnDialogAccepted();
  else
    dialog->OnDialogCanceled();
}

}  // namespace web_app
