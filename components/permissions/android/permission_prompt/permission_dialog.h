// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_H_

#include "components/permissions/android/permission_prompt/permission_prompt_android.h"

namespace content {
class WebContents;
}

namespace permissions {

class PermissionDialog : public PermissionPromptAndroid {
 public:
  PermissionDialog(content::WebContents* web_contents, Delegate* delegate);

  PermissionDialog(const PermissionDialog&) = delete;
  PermissionDialog& operator=(const PermissionDialog&) = delete;

  ~PermissionDialog() override;

  PermissionPromptDisposition GetPromptDisposition() const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_H_
