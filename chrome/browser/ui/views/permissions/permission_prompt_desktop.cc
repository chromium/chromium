// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/tabs/public/tab_interface.h"

namespace {}  // namespace

PermissionPromptDesktop::PermissionPromptDesktop(
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate) {
  if (!browser_) {
    UpdateBrowser();
  }
}

PermissionPromptDesktop::~PermissionPromptDesktop() = default;

bool PermissionPromptDesktop::UpdateBrowser() {
  tabs::TabInterface* tab =
      web_contents_ ? tabs::TabInterface::MaybeGetFromContents(web_contents_)
                    : nullptr;
  BrowserWindowInterface* current_browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!current_browser) {
    current_browser = webui::GetBrowserWindowInterface(web_contents_);
  }
  // Browser for |web_contents_| might change when for example the tab was
  // dragged to another window.
  bool was_browser_changed = false;
  if (current_browser != browser_) {
    browser_ = current_browser;
    was_browser_changed = true;
  }

  return was_browser_changed;
}

bool PermissionPromptDesktop::UpdateAnchor() {
  UpdateBrowser();
  return true;
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptDesktop::GetTabSwitchingBehavior() {
  return permissions::PermissionPrompt::TabSwitchingBehavior::
      kDestroyPromptButKeepRequestPending;
}

std::optional<gfx::Rect> PermissionPromptDesktop::GetViewBoundsInScreen()
    const {
  return std::nullopt;
}

views::Widget* PermissionPromptDesktop::GetPromptBubbleWidgetForTesting() {
  return nullptr;
}


std::vector<permissions::ElementAnchoredBubbleVariant>
PermissionPromptDesktop::GetPromptVariants() const {
  return {};
}

std::optional<permissions::feature_params::PermissionElementPromptPosition>
PermissionPromptDesktop::GetPromptPosition() const {
  return std::nullopt;
}

bool PermissionPromptDesktop::IsAskPrompt() const {
  return true;
}

LocationBar* PermissionPromptDesktop::GetLocationBar() {
  BrowserWindow* browser_window =
      browser_ ? BrowserWindow::FromBrowser(browser_) : nullptr;
  return browser_window ? browser_window->GetLocationBar() : nullptr;
}
