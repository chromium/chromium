// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_MESSAGE_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_MESSAGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/permissions_client.h"

namespace content {
class WebContents;
}

namespace permissions {

class PermissionMessage : public PermissionPromptAndroid {
 public:
  PermissionMessage(const PermissionMessage&) = delete;
  PermissionMessage& operator=(const PermissionMessage&) = delete;
  ~PermissionMessage() override;

  static std::unique_ptr<PermissionMessage> Create(
      content::WebContents* web_contents,
      Delegate* delegate);

  PermissionPromptDisposition GetPromptDisposition() const override;

 private:
  PermissionMessage(content::WebContents* web_contents, Delegate* delegate);

  std::unique_ptr<PermissionsClient::PermissionMessageDelegate>
      message_delegate_;

  base::WeakPtrFactory<PermissionMessage> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_MESSAGE_H_
