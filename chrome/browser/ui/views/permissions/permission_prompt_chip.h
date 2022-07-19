// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_H_

#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"

class Browser;

namespace content {
class WebContents;
}

class PermissionPromptChip : public PermissionPromptDesktop {
 public:
  PermissionPromptChip(Browser* browser,
                       content::WebContents* web_contents,
                       Delegate* delegate);
  ~PermissionPromptChip() override;
  PermissionPromptChip(const PermissionPromptChip&) = delete;
  PermissionPromptChip& operator=(const PermissionPromptChip&) = delete;

  // permissions::PermissionPrompt:
  void UpdateAnchor() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  views::Widget* GetPromptBubbleWidgetForTesting() override;

 private:
  void FinalizeChip();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_H_
