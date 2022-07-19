// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_H_

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"

class Browser;

namespace content {
class WebContents;
}

class PermissionPromptBubble : public PermissionPromptDesktop,
                               public views::WidgetObserver {
 public:
  PermissionPromptBubble(Browser* browser,
                         content::WebContents* web_contents,
                         Delegate* delegate);
  ~PermissionPromptBubble() override;
  PermissionPromptBubble(const PermissionPromptBubble&) = delete;
  PermissionPromptBubble& operator=(const PermissionPromptBubble&) = delete;

  void ShowBubble();
  void CleanUpPromptBubble();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // permissions::PermissionPrompt:
  void UpdateAnchor() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  views::Widget* GetPromptBubbleWidgetForTesting() override;

 private:
  // The popup bubble. Not owned by this class; it will delete itself when a
  // decision is made.
  raw_ptr<PermissionPromptBubbleView> prompt_bubble_ = nullptr;

  base::TimeTicks permission_requested_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_H_
