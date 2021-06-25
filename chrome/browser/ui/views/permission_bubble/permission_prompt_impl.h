// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "components/permissions/permission_prompt.h"

class Browser;
class PermissionPromptBubbleView;

namespace content {
class WebContents;
}  // namespace content

// This object will create or trigger UI to reflect that a website is requesting
// a permission. The UI is usually a popup bubble, but may instead be a location
// bar icon (the "quiet" prompt).
class PermissionPromptImpl : public permissions::PermissionPrompt,
                             public views::WidgetObserver {
 public:
  PermissionPromptImpl(Browser* browser,
                       content::WebContents* web_contents,
                       Delegate* delegate);
  ~PermissionPromptImpl() override;

  // permissions::PermissionPrompt:
  void UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  PermissionPromptBubbleView* prompt_bubble_for_testing() {
    if (prompt_bubble_)
      return prompt_bubble_;
    return permission_chip_ ? permission_chip_->prompt_bubble_for_testing()
                            : nullptr;
  }

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  LocationBarView* GetLocationBarView();

  void ShowBubble();

  void ShowChipUI();

  bool ShouldCurrentRequestUseChipUI();

  // The popup bubble. Not owned by this class; it will delete itself when a
  // decision is made.
  PermissionPromptBubbleView* prompt_bubble_;

  // The web contents whose location bar should show the quiet prompt.
  content::WebContents* web_contents_;

  PermissionPromptStyle prompt_style_;

  PermissionChip* permission_chip_ = nullptr;

  permissions::PermissionPrompt::Delegate* const delegate_;

  Browser* browser_;

  base::TimeTicks permission_requested_time_;

  DISALLOW_COPY_AND_ASSIGN(PermissionPromptImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_
