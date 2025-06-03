// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/permissions/android/permission_prompt/embedded_permission_prompt_android.h"
#include "components/permissions/android/permission_prompt/permission_dialog.h"
#include "components/permissions/android/permission_prompt/permission_message.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  if (permissions::PermissionUtil::
          ShouldCurrentRequestUsePermissionElementSecondaryUI(delegate)) {
    if (auto embedded_prompt =
            EmbeddedPermissionPromptAndroid::Create(web_contents, delegate)) {
      return embedded_prompt;
    }
  }
  // Quiet UI (non-modal, less intrusive) is preferred over loud one, if
  // necessary conditions are met.
  auto message_ui = PermissionMessage::Create(web_contents, delegate);
  if (message_ui) {
    return message_ui;
  }

  return PermissionDialog::Create(web_contents, delegate);
}

}  // namespace permissions
