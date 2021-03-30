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
      browser_(browser),
      permission_requested_time_(base::TimeTicks::Now()) {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents_);
  if (manager->ShouldCurrentRequestUseQuietUI()) {
    prompt_style_ = PermissionPromptStyle::kQuiet;
    // Shows the prompt as an indicator in the right side of the omnibox.
    content_settings::UpdateLocationBarUiForWebContents(web_contents_);
  } else {
    LocationBarView* lbv = GetLocationBarView();
    if (lbv && lbv->IsDrawn() && ShouldCurrentRequestUseChipUI()) {
      ShowChipUI();
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
  switch (prompt_style_) {
    case PermissionPromptStyle::kBubbleOnly:
      DCHECK(!permission_chip_);
      if (prompt_bubble_)
        prompt_bubble_->GetWidget()->Close();
      break;
    case PermissionPromptStyle::kChip:
      DCHECK(!prompt_bubble_);
      DCHECK(permission_chip_);
      permission_chip_->FinalizeRequest();
      permission_chip_ = nullptr;
      break;
    case PermissionPromptStyle::kQuiet:
      DCHECK(!prompt_bubble_);
      DCHECK(!permission_chip_);
      content_settings::UpdateLocationBarUiForWebContents(web_contents_);
      break;
  }

  CHECK(!IsInObserverList());
}

void PermissionPromptImpl::UpdateAnchor() {
  Browser* current_browser = chrome::FindBrowserWithWebContents(web_contents_);
  // Browser for |web_contents_| might change when for example the tab was
  // dragged to another window.
  bool was_browser_changed = false;
  if (current_browser != browser_) {
    browser_ = current_browser;
    was_browser_changed = true;
  }
  LocationBarView* lbv = GetLocationBarView();
  const bool is_location_bar_drawn = lbv && lbv->IsDrawn();
  switch (prompt_style_) {
    case PermissionPromptStyle::kBubbleOnly:
      DCHECK(!permission_chip_);
      // TODO(crbug.com/1175231): Investigate why prompt_bubble_ can be null
      // here. Early return is preventing the crash from happening but we still
      // don't know the reason why it is null here and cannot reproduce it.
      if (!prompt_bubble_)
        return;

      if (ShouldCurrentRequestUseChipUI() && is_location_bar_drawn) {
        // Change prompt style to chip to avoid dismissing request while
        // switching UI style.
        prompt_bubble_->SetPromptStyle(PermissionPromptStyle::kChip);
        prompt_bubble_->GetWidget()->Close();
        ShowChipUI();
        permission_chip_->OpenBubble();
      } else {
        // If |browser_| changed, recreate bubble for correct browser.
        if (was_browser_changed) {
          prompt_bubble_->GetWidget()->CloseWithReason(
              views::Widget::ClosedReason::kUnspecified);
          ShowBubble();
        } else {
          prompt_bubble_->UpdateAnchorPosition();
        }
      }
      break;
    case PermissionPromptStyle::kChip:
      DCHECK(!prompt_bubble_);
      DCHECK(permission_chip_);
      permission_chip_ = lbv->permission_chip();
      if (!permission_chip_->GetActiveRequest())
        permission_chip_->DisplayRequest(delegate_);
      // If there is fresh pending request shown as chip UI and location bar
      // isn't visible anymore, show bubble UI instead.
      if (!permission_chip_->is_fully_collapsed() && !is_location_bar_drawn) {
        permission_chip_->FinalizeRequest();
        permission_chip_ = nullptr;
        ShowBubble();
      }
      break;
    case PermissionPromptStyle::kQuiet:
      break;
  }
}

LocationBarView* PermissionPromptImpl::GetLocationBarView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  return browser_view ? browser_view->GetLocationBarView() : nullptr;
}

void PermissionPromptImpl::ShowChipUI() {
  LocationBarView* lbv = GetLocationBarView();
  DCHECK(lbv);

  permission_chip_ = lbv->permission_chip();
  permission_chip_->DisplayRequest(delegate_);
  prompt_style_ = PermissionPromptStyle::kChip;
}

void PermissionPromptImpl::ShowBubble() {
  prompt_style_ = PermissionPromptStyle::kBubbleOnly;
  prompt_bubble_ = new PermissionPromptBubbleView(
      browser_, delegate_, permission_requested_time_, prompt_style_);
  prompt_bubble_->Show();
  prompt_bubble_->GetWidget()->AddObserver(this);
}

bool PermissionPromptImpl::ShouldCurrentRequestUseChipUI() {
  if (!base::FeatureList::IsEnabled(permissions::features::kPermissionChip))
    return false;

  std::vector<permissions::PermissionRequest*> requests = delegate_->Requests();
  return std::all_of(requests.begin(), requests.end(), [](auto* request) {
    return request->GetChipText().has_value();
  });
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
