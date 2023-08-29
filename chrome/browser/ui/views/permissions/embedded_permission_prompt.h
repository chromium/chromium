// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_

#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"

class Browser;

namespace content {
class WebContents;
}

class EmbeddedPermissionPrompt : public PermissionPromptDesktop {
 public:
  EmbeddedPermissionPrompt(Browser* browser,
                           content::WebContents* web_contents,
                           Delegate* delegate);
  ~EmbeddedPermissionPrompt() override;
  EmbeddedPermissionPrompt(const EmbeddedPermissionPrompt&) = delete;
  EmbeddedPermissionPrompt& operator=(const EmbeddedPermissionPrompt&) = delete;

  // permissions::PermissionPrompt:
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
