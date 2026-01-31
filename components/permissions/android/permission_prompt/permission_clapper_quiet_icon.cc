// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_clapper_quiet_icon.h"

#include "base/android/jni_array.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/android/permission_prompt/permission_clapper_quiet_icon_delegate.h"
#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace permissions {

PermissionClapperQuietIcon::PermissionClapperQuietIcon(
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptAndroid(web_contents, delegate),
      delegate_(
          std::make_unique<PermissionClapperQuietIconDelegate>(web_contents)) {
  // Quiet Clapper should only be enabled for Notification requests.
  CHECK_EQ(delegate->Requests()[0]->GetContentSettingsType(),
           ContentSettingsType::NOTIFICATIONS);
}

PermissionClapperQuietIcon::~PermissionClapperQuietIcon() = default;
bool PermissionClapperQuietIcon::UpdateAnchor() {
  return true;
}

PermissionPrompt::TabSwitchingBehavior
PermissionClapperQuietIcon::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kDestroyPromptButKeepRequestPending;
}

PermissionPromptDisposition PermissionClapperQuietIcon::GetPromptDisposition()
    const {
  return PermissionPromptDisposition::LOCATION_BAR_LEFT_CLAPPER_QUIET_ICON;
}

}  // namespace permissions
