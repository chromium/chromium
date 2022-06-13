// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "components/permissions/permission_prompt.h"

class Browser;
class LocationBarView;

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

  PermissionPromptImpl(const PermissionPromptImpl&) = delete;
  PermissionPromptImpl& operator=(const PermissionPromptImpl&) = delete;

  ~PermissionPromptImpl() override;

  // permissions::PermissionPrompt:
  void UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  void CleanUpPromptBubble();

  views::Widget* GetPromptBubbleWidgetForTesting();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  bool IsLocationBarDisplayed();
  void SelectPwaPrompt();
  void SelectNormalPrompt();
  void SelectQuietPrompt();
  LocationBarView* GetLocationBarView();
  void ShowQuietIcon();
  void ShowBubble();
  void ShowChip();
  bool ShouldCurrentRequestUseChip();
  bool ShouldCurrentRequestUseQuietChip();
  void FinalizeChip();

  // The popup bubble. Not owned by this class; it will delete itself when a
  // decision is made.
  raw_ptr<PermissionPromptBubbleView> prompt_bubble_;

  // The web contents whose location bar should show the quiet prompt.
  raw_ptr<content::WebContents> web_contents_;

  PermissionPromptStyle prompt_style_;

  const raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  raw_ptr<Browser> browser_;

  base::TimeTicks permission_requested_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_
