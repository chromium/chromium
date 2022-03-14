// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_ANDROID_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"

namespace content {
class WebContents;
}
namespace infobars {
class InfoBar;
}

namespace permissions {

class PermissionPromptAndroid : public permissions::PermissionPrompt,
                                public infobars::InfoBarManager::Observer {
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

  // permissions::PermissionPrompt:
  void UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  void Closing();
  void Accept();
  void Deny();
  void SetManageClicked();
  void SetLearnMoreClicked();
  bool ShouldCurrentRequestUseQuietUI();
  absl::optional<PermissionUiSelector::QuietUiReason> ReasonForUsingQuietUi()
      const;

  // We show one permission at a time except for grouped mic+camera, for which
  // we still have a single icon and message text.
  size_t PermissionCount() const;
  ContentSettingsType GetContentSettingType(size_t position) const;
  int GetIconId() const;
  std::u16string GetTitleText() const;
  std::u16string GetMessageText() const;

  const content::WebContents* web_contents() { return web_contents_; }

  // InfoBar::Manager:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

 private:
  // PermissionPromptAndroid is owned by PermissionRequestManager, so it should
  // be safe to hold a raw WebContents pointer here because this class is
  // destroyed before the WebContents.
  const raw_ptr<content::WebContents> web_contents_;
  // |delegate_| is the PermissionRequestManager, which owns this object.
  const raw_ptr<Delegate> delegate_;

  // The infobar used to display the permission request, if displayed in that
  // format. Never assume that this pointer is currently alive.
  raw_ptr<infobars::InfoBar> permission_infobar_;

  // Message UI is alternative to infobars. So it should be impossible that
  // both |message_delegate_| and |permission_infobar_| are non-null at the
  // same moment.
  std::unique_ptr<PermissionsClient::PermissionMessageDelegate>
      message_delegate_;

  permissions::PermissionPromptDisposition prompt_disposition_ =
      permissions::PermissionPromptDisposition::NOT_APPLICABLE;

  base::WeakPtrFactory<PermissionPromptAndroid> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_ANDROID_H_
