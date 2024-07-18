// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/permissions/permission_prompt.h"

class Browser;
class LocationBarView;

namespace content {
class WebContents;
}  // namespace content

// This object will create or trigger UI to reflect that a website is requesting
// a permission. The UI is usually a popup bubble, but may instead be a location
// bar icon (the "quiet" prompt).
class PermissionPromptDesktop : public permissions::PermissionPrompt {
 public:
  PermissionPromptDesktop(Browser* browser,
                          content::WebContents* web_contents,
                          Delegate* delegate);

  PermissionPromptDesktop(const PermissionPromptDesktop&) = delete;
  PermissionPromptDesktop& operator=(const PermissionPromptDesktop&) = delete;

  ~PermissionPromptDesktop() override;

  // permissions::PermissionPrompt:
  bool UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override = 0;
  bool IsAskPrompt() const override;
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;
  bool ShouldFinalizeRequestAfterDecided() const override;
  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const override;
  std::optional<permissions::feature_params::PermissionElementPromptPosition>
  GetPromptPosition() const override;

  virtual views::Widget* GetPromptBubbleWidgetForTesting();

 protected:
  LocationBarView* GetLocationBarView();

  Browser* browser() const { return browser_; }
  bool UpdateBrowser();

  permissions::PermissionPrompt::Delegate* delegate() const {
    return delegate_;
  }
  content::WebContents* web_contents() const { return web_contents_; }

 private:
  // The web contents whose location bar should show the quiet prompt.
  raw_ptr<content::WebContents> web_contents_;

  const raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_DESKTOP_H_
