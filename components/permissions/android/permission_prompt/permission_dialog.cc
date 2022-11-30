// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_dialog.h"

#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"

namespace permissions {

PermissionDialog::PermissionDialog(content::WebContents* web_contents,
                                   Delegate* delegate)
    : PermissionPromptAndroid(web_contents, delegate) {
  DCHECK(web_contents);

  PermissionDialogDelegate::Create(web_contents, this);
}

PermissionDialog::~PermissionDialog() = default;

PermissionPromptDisposition PermissionDialog::GetPromptDisposition() const {
  return PermissionPromptDisposition::MODAL_DIALOG;
}

}  // namespace permissions
