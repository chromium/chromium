// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/permissions/android/permission_prompt/permission_dialog.h"
#include "components/permissions/android/permission_prompt/permission_message.h"
#include "components/permissions/android/permission_prompt/permission_prompt_infobar.h"
#include "components/permissions/permission_prompt.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  // Quiet UI (non-modal, less intrusive) is preferred over loud one, if
  // necessary conditions are met. The message UI is preferred over the infobar
  // UI.
  auto message_ui = PermissionMessage::Create(web_contents, delegate);
  if (message_ui)
    return message_ui;

  auto infobar = PermissionPromptInfoBar::Create(web_contents, delegate);
  if (infobar)
    return infobar;

  return std::make_unique<PermissionDialog>(web_contents, delegate);
}

}  // namespace permissions
