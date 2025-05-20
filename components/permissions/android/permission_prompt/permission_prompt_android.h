// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_ANDROID_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"
#include "components/permissions/embedded_permission_prompt_flow_model.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}

namespace permissions {

// Virtual class that is the base class for all Android permission prompts.
class PermissionPromptAndroid : public PermissionPrompt {
 public:
  PermissionPromptAndroid(content::WebContents* web_contents,
                          Delegate* delegate);

  PermissionPromptAndroid(const PermissionPromptAndroid&) = delete;
  PermissionPromptAndroid& operator=(const PermissionPromptAndroid&) = delete;

  // Expect to be destroyed (and the UI needs to go) when:
  // 1. A navigation happens, tab/webcontents is being closed; with the current
  //    GetTabSwitchingBehavior() implementation, this instance survives the tab
  //    being backgrounded.
  // 2. The permission request is resolved (accept, deny, dismiss).
  // 3. A higher priority request comes in.
  ~PermissionPromptAndroid() override;

  // PermissionPrompt:
  bool UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;
  bool ShouldFinalizeRequestAfterDecided() const override;
  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const override;
  bool IsAskPrompt() const override;
  std::optional<permissions::feature_params::PermissionElementPromptPosition>
  GetPromptPosition() const override;

  virtual EmbeddedPermissionPromptFlowModel::Variant GetEmbeddedPromptVariant()
      const;
  virtual void Closing();
  virtual void Accept();
  virtual void AcceptThisTime();
  virtual void Acknowledge() {}
  virtual void Deny();
  virtual void Resumed() {}
  virtual void SystemSettingsShown() {}
  virtual void SystemPermissionResolved(bool accepted) {}
  void SetManageClicked();
  void SetLearnMoreClicked();
  virtual bool ShouldCurrentRequestUseQuietUI();
  virtual std::optional<PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const;
  virtual base::android::ScopedJavaLocalRef<jstring> GetPositiveButtonText(
      JNIEnv* env,
      bool is_one_time) const;
  virtual base::android::ScopedJavaLocalRef<jstring> GetNegativeButtonText(
      JNIEnv* env,
      bool is_one_time) const;
  virtual base::android::ScopedJavaLocalRef<jstring>
  GetPositiveEphemeralButtonText(JNIEnv* env, bool is_one_time) const;
  virtual base::android::ScopedJavaLocalRef<jobjectArray> GetRadioButtonTexts(
      JNIEnv* env,
      bool is_one_time) const;

  // We show one permission at a time except for grouped mic+camera, for which
  // we still have a single icon and message text.
  size_t PermissionCount() const;
  ContentSettingsType GetContentSettingType(size_t position) const;
  virtual int GetIconId() const;
  virtual PermissionRequest::AnnotatedMessageText GetAnnotatedMessageText()
      const;
  virtual bool ShouldUseRequestingOriginFavicon() const;
  virtual const std::vector<base::WeakPtr<permissions::PermissionRequest>>&
  Requests() const;
  GURL GetRequestingOrigin() const;
  content::WebContents* web_contents() const { return web_contents_; }
  PermissionDialogDelegate* permission_dialog_delegate() const {
    return permission_dialog_delegate_.get();
  }
  void ClearPermissionDialogDelegate() { permission_dialog_delegate_.reset(); }

  base::android::ScopedJavaLocalRef<jintArray> GetContentSettingTypes(
      JNIEnv* env) const;
  base::android::ScopedJavaLocalRef<jintArray> GetBoldRanges(JNIEnv* env) const;

  bool IsShowing() const { return this == delegate()->GetCurrentPrompt(); }

 protected:
  Delegate* delegate() const { return delegate_; }

  void CreatePermissionDialogDelegate() {
    permission_dialog_delegate_ =
        PermissionDialogDelegate::Create(web_contents_, this);
  }

  // Check if grouped permission requests can only be Mic+Camera, Camera+Mic.
  void CheckValidRequestGroup(
      const std::vector<base::WeakPtr<PermissionRequest>>& requests) const;

 private:
  // PermissionPromptAndroid is owned by PermissionRequestManager, so it should
  // be safe to hold a raw WebContents pointer here because this class is
  // destroyed before the WebContents.
  const raw_ptr<content::WebContents> web_contents_;

  // |delegate_| is the PermissionRequestManager, which owns this object.
  const raw_ptr<Delegate> delegate_;

  std::vector<base::WeakPtr<PermissionRequest>> requests_;

  // Owns a `PermissionDialogDelegate` object.
  std::unique_ptr<PermissionDialogDelegate> permission_dialog_delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_ANDROID_H_
