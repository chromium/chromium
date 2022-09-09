// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_QUIET_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_QUIET_ICON_H_

#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"

class Browser;

namespace content {
class WebContents;
}

class PermissionPromptQuietIcon : public PermissionPromptDesktop {
 public:
  PermissionPromptQuietIcon(Browser* browser,
                            content::WebContents* web_contents,
                            Delegate* delegate);
  ~PermissionPromptQuietIcon() override;
  PermissionPromptQuietIcon(const PermissionPromptQuietIcon&) = delete;
  PermissionPromptQuietIcon& operator=(const PermissionPromptQuietIcon&) =
      delete;

  // permissions::PermissionPrompt:
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_QUIET_ICON_H_
