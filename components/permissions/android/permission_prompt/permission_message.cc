// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_message.h"

#include "base/memory/ptr_util.h"

namespace {
permissions::PermissionPromptDisposition permission_prompt_disposition =
    permissions::PermissionPromptDisposition::NOT_APPLICABLE;
}  // namespace

namespace permissions {

PermissionMessage::PermissionMessage(content::WebContents* web_contents,
                                     Delegate* delegate)
    : PermissionPromptAndroid(web_contents, delegate) {
  auto* permission_client = PermissionsClient::Get();
  message_delegate_ = permission_client->MaybeCreateMessageUI(
      web_contents, GetContentSettingType(0u /* position */),
      weak_factory_.GetWeakPtr());
}

PermissionMessage::~PermissionMessage() = default;

// static
std::unique_ptr<PermissionMessage> PermissionMessage::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  auto prompt = base::WrapUnique(new PermissionMessage(web_contents, delegate));
  if (prompt->message_delegate_) {
    // The MessageUI can be used to display a quiet or a loud prompt.
    permission_prompt_disposition =
        delegate->ShouldCurrentRequestUseQuietUI()
            ? PermissionPromptDisposition::MESSAGE_UI
            : PermissionPromptDisposition::MESSAGE_UI_LOUD;
    return prompt;
  }
  return nullptr;
}

PermissionPromptDisposition PermissionMessage::GetPromptDisposition() const {
  return permission_prompt_disposition;
}

}  // namespace permissions
