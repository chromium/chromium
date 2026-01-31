// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_CLAPPER_QUIET_ICON_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_CLAPPER_QUIET_ICON_H_

#include <memory>

#include "components/permissions/android/permission_prompt/permission_prompt_android.h"

namespace content {
class WebContents;
}

namespace permissions {

class PermissionClapperQuietIconDelegate;

// A quiet permission prompt for notifications on Android that only
// updates the omnibox icon to show a crossed out bell icon.
class PermissionClapperQuietIcon : public PermissionPromptAndroid {
 public:
  PermissionClapperQuietIcon(content::WebContents* web_contents,
                             Delegate* delegate);
  ~PermissionClapperQuietIcon() override;

  // PermissionPromptAndroid:
  bool UpdateAnchor() override;
  PermissionPromptDisposition GetPromptDisposition() const override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;

 private:
  std::unique_ptr<PermissionClapperQuietIconDelegate> delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_CLAPPER_QUIET_ICON_H_
