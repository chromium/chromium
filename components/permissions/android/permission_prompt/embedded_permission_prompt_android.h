// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_EMBEDDED_PERMISSION_PROMPT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_EMBEDDED_PERMISSION_PROMPT_ANDROID_H_

#include <memory>

#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/embedded_permission_prompt_flow_model.h"

namespace content {
class WebContents;
}

namespace permissions {

// Permission prompt for PEPC requests (permission requests when users click on
// <permission> element).
class EmbeddedPermissionPromptAndroid : public PermissionPromptAndroid {
 public:
  EmbeddedPermissionPromptAndroid(content::WebContents* web_contents,
                                  Delegate* delegate);

  EmbeddedPermissionPromptAndroid(const EmbeddedPermissionPromptAndroid&) =
      delete;
  EmbeddedPermissionPromptAndroid& operator=(
      const EmbeddedPermissionPromptAndroid&) = delete;

  ~EmbeddedPermissionPromptAndroid() override;

  // PermissionPrompt:
  PermissionPromptDisposition GetPromptDisposition() const override;
  bool ShouldFinalizeRequestAfterDecided() const override;

  // PermissionPromptAndroid:
  EmbeddedPermissionPromptFlowModel::Variant GetEmbeddedPromptVariant()
      const override;
  void Closing() override;
  void Accept() override;
  void AcceptThisTime() override;
  void Deny() override;
  bool ShouldCurrentRequestUseQuietUI() override;
  std::optional<PermissionUiSelector::QuietUiReason> ReasonForUsingQuietUi()
      const override;
  PermissionRequest::AnnotatedMessageText GetAnnotatedMessageText()
      const override;
  bool ShouldUseRequestingOriginFavicon() const override;

 private:
  // Decide to destroy the current dialog or update the dialog with new screen
  // variant.
  void MaybeUpdateDialogWithNewScreenVariant();

  std::unique_ptr<EmbeddedPermissionPromptFlowModel> prompt_model_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_EMBEDDED_PERMISSION_PROMPT_ANDROID_H_
