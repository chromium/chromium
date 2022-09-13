// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_prompt_infobar.h"

#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

PermissionPromptInfoBar::PermissionPromptInfoBar(
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptAndroid(web_contents, delegate) {
  auto* permission_client = PermissionsClient::Get();
  permission_prompt_infobar_ = permission_client->MaybeCreateInfoBar(
      web_contents, GetContentSettingType(0u /* position */),
      weak_factory_.GetWeakPtr());
  if (permission_prompt_infobar_)
    permission_client->GetInfoBarManager(web_contents)->AddObserver(this);
}

PermissionPromptInfoBar::~PermissionPromptInfoBar() {
  infobars::InfoBarManager* infobar_manager =
      PermissionsClient::Get()->GetInfoBarManager(web_contents());
  if (!infobar_manager)
    return;
  // RemoveObserver before RemoveInfoBar to not get notified about the removal
  // of the `permission_prompt_infobar_` infobar.
  infobar_manager->RemoveObserver(this);
  if (permission_prompt_infobar_) {
    infobar_manager->RemoveInfoBar(permission_prompt_infobar_);
  }
}

// static
std::unique_ptr<PermissionPromptInfoBar> PermissionPromptInfoBar::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  auto prompt =
      base::WrapUnique(new PermissionPromptInfoBar(web_contents, delegate));
  if (prompt->permission_prompt_infobar_)
    return prompt;
  return nullptr;
}

void PermissionPromptInfoBar::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                               bool animate) {
  if (infobar != permission_prompt_infobar_)
    return;

  permission_prompt_infobar_ = nullptr;
  infobars::InfoBarManager* infobar_manager =
      PermissionsClient::Get()->GetInfoBarManager(web_contents());
  if (infobar_manager)
    infobar_manager->RemoveObserver(this);
}

void PermissionPromptInfoBar::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  permission_prompt_infobar_ = nullptr;
  manager->RemoveObserver(this);
}

PermissionPromptDisposition PermissionPromptInfoBar::GetPromptDisposition()
    const {
  return PermissionPromptDisposition::MINI_INFOBAR;
}

}  // namespace permissions
