// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_ANDROID_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
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

  void Closing();
  void Accept();
  void AcceptThisTime();
  void Deny();
  void SetManageClicked();
  void SetLearnMoreClicked();
  bool ShouldCurrentRequestUseQuietUI();
  std::optional<PermissionUiSelector::QuietUiReason> ReasonForUsingQuietUi()
      const;

  // We show one permission at a time except for grouped mic+camera, for which
  // we still have a single icon and message text.
  size_t PermissionCount() const;
  ContentSettingsType GetContentSettingType(size_t position) const;
  int GetIconId() const;

  PermissionRequest::AnnotatedMessageText GetAnnotatedMessageText() const;

  bool ShouldUseRequestingOriginFavicon() const;

  GURL GetRequestingOrigin() const;
  content::WebContents* web_contents() { return web_contents_; }

 private:
  // PermissionPromptAndroid is owned by PermissionRequestManager, so it should
  // be safe to hold a raw WebContents pointer here because this class is
  // destroyed before the WebContents.
  const raw_ptr<content::WebContents> web_contents_;

  // |delegate_| is the PermissionRequestManager, which owns this object.
  const raw_ptr<Delegate> delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_ANDROID_H_
