// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_DIALOG_H_
#define COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_DIALOG_H_

#include "components/permissions/ios/content/permission_prompt/permission_prompt_ios.h"

namespace content {
class WebContents;
}

namespace permissions {

class PermissionDialog : public PermissionPromptIOS {
 public:
  PermissionDialog(content::WebContents* web_contents, Delegate* delegate);

  PermissionDialog(const PermissionDialog&) = delete;
  PermissionDialog& operator=(const PermissionDialog&) = delete;

  ~PermissionDialog() override = default;

  static std::unique_ptr<PermissionDialog> Create(
      content::WebContents* web_contents,
      Delegate* delegate);

  // PermissionPrompt:
  PermissionPromptDisposition GetPromptDisposition() const override;

  // PermissionPromptIOS:
  NSString* GetPositiveButtonText(bool is_one_time) const override;
  NSString* GetNegativeButtonText(bool is_one_time) const override;
  NSString* GetPositiveEphemeralButtonText(bool is_one_time) const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_DIALOG_H_
