// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

#include <memory>

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "components/permissions/features.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_frame_view.h"

std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    DLOG(WARNING) << "Permission prompt suppressed because the WebContents is "
                     "not attached to any Browser window.";
    return nullptr;
  }
  return std::make_unique<PermissionPromptImpl>(browser, web_contents,
                                                delegate);
}

PermissionPromptImpl::PermissionPromptImpl(Browser* browser,
                                           content::WebContents* web_contents,
                                           Delegate* delegate)
    : prompt_bubble_(nullptr),
      web_contents_(web_contents),
      delegate_(delegate),
      browser_(browser) {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents_);
  if (manager->ShouldCurrentRequestUseQuietUI()) {
    prompt_style_ = PermissionPromptStyle::kQuiet;
    // Shows the prompt as an indicator in the right side of the omnibox.
    content_settings::UpdateLocationBarUiForWebContents(web_contents_);
  } else {
    LocationBarView* lbv = GetLocationBarView();
    std::vector<permissions::PermissionRequest*> requests =
        delegate->Requests();
    const bool should_use_chip_ui = std::all_of(
        requests.begin(), requests.end(),
        [](auto* request) { return request->GetChipText().has_value(); });
    if (base::FeatureList::IsEnabled(permissions::features::kPermissionChip) &&
        lbv && lbv->IsDrawn() && should_use_chip_ui) {
      permission_chip_ = lbv->permission_chip();
      permission_chip_->Show(delegate);
      prompt_style_ = PermissionPromptStyle::kChip;
    } else {
      ShowBubble();
    }
  }
}

void PermissionPromptImpl::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, prompt_bubble_->GetWidget());
  widget->RemoveObserver(this);
  prompt_bubble_ = nullptr;
}

PermissionPromptImpl::~PermissionPromptImpl() {
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();

  if (prompt_style_ == PermissionPromptStyle::kQuiet) {
    // Hides the quiet prompt.
    content_settings::UpdateLocationBarUiForWebContents(web_contents_);
  }

  if (prompt_style_ == PermissionPromptStyle::kChip && permission_chip_) {
    permission_chip_->Hide();
  }
  CHECK(!IsInObserverList());
}

void PermissionPromptImpl::UpdateAnchorPosition() {
  // TODO(olesiamarukhno): Figure out if we need to switch UI styles (chip to
  // bubble and vice versa) here.
  if (prompt_bubble_)
    prompt_bubble_->UpdateAnchorPosition();
}

LocationBarView* PermissionPromptImpl::GetLocationBarView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  return browser_view ? browser_view->GetLocationBarView() : nullptr;
}

void PermissionPromptImpl::ShowBubble() {
  prompt_style_ = PermissionPromptStyle::kBubbleOnly;
  prompt_bubble_ = new PermissionPromptBubbleView(
      browser_, delegate_, base::TimeTicks::Now(), prompt_style_);
  prompt_bubble_->Show();
  prompt_bubble_->GetWidget()->AddObserver(this);
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptImpl::GetTabSwitchingBehavior() {
  return permissions::PermissionPrompt::TabSwitchingBehavior::
      kDestroyPromptButKeepRequestPending;
}

permissions::PermissionPromptDisposition
PermissionPromptImpl::GetPromptDisposition() const {
  switch (prompt_style_) {
    case PermissionPromptStyle::kBubbleOnly:
      return permissions::PermissionPromptDisposition::ANCHORED_BUBBLE;
    case PermissionPromptStyle::kChip:
      return permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP;
    case PermissionPromptStyle::kQuiet: {
      permissions::PermissionRequestManager* manager =
          permissions::PermissionRequestManager::FromWebContents(web_contents_);
      return permissions::NotificationPermissionUiSelector::
                     ShouldSuppressAnimation(manager->ReasonForUsingQuietUi())
                 ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_RIGHT_STATIC_ICON
                 : permissions::PermissionPromptDisposition::
                       LOCATION_BAR_RIGHT_ANIMATED_ICON;
    }
  }
}
