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

  static std::unique_ptr<EmbeddedPermissionPromptAndroid> Create(
      content::WebContents* web_contents,
      Delegate* delegate);

  // PermissionPrompt:
  PermissionPromptDisposition GetPromptDisposition() const override;
  bool ShouldFinalizeRequestAfterDecided() const override;
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;
  bool IsAskPrompt() const override;

  // PermissionPromptAndroid:
  EmbeddedPermissionPromptFlowModel::Variant GetEmbeddedPromptVariant()
      const override;
  void Closing() override;
  void Accept() override;
  void AcceptThisTime() override;
  void Acknowledge() override;
  void Deny() override;
  void Resumed() override;
  void SystemSettingsShown() override;
  void SystemPermissionResolved(bool accepted) override;
  bool ShouldCurrentRequestUseQuietUI() override;
  std::optional<PermissionUiSelector::QuietUiReason> ReasonForUsingQuietUi()
      const override;
  PermissionRequest::AnnotatedMessageText GetAnnotatedMessageText()
      const override;
  base::android::ScopedJavaLocalRef<jstring> GetPositiveButtonText(
      JNIEnv* env,
      bool is_one_time) const override;
  base::android::ScopedJavaLocalRef<jstring> GetNegativeButtonText(
      JNIEnv* env,
      bool is_one_time) const override;
  base::android::ScopedJavaLocalRef<jstring> GetPositiveEphemeralButtonText(
      JNIEnv* env,
      bool is_one_time) const override;
  base::android::ScopedJavaLocalRef<jobjectArray> GetRadioButtonTexts(
      JNIEnv* env,
      bool is_one_time) const override;

  bool ShouldUseRequestingOriginFavicon() const override;
  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const override;
  const std::vector<base::WeakPtr<permissions::PermissionRequest>>& Requests()
      const override;
  int GetIconId() const override;

 private:
  // Decide to destroy the current dialog or update the dialog with new screen
  // variant.
  void MaybeUpdateDialogWithNewScreenVariant();

  PermissionRequest::AnnotatedMessageText
  GetDialogAnnotatedMessageTextWithOrigin(int message_id) const;

  std::u16string GetPermissionNameTextFragment() const;

  std::unique_ptr<EmbeddedPermissionPromptFlowModel> prompt_model_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_EMBEDDED_PERMISSION_PROMPT_ANDROID_H_
