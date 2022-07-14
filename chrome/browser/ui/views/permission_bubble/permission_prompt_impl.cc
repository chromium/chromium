// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

#include <memory>

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/permission_chip.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_frame_view.h"

namespace {

bool IsFullScreenMode(Browser* browser) {
  DCHECK(browser);

  // PWA uses the title bar as a substitute for LocationBarView.
  if (web_app::AppBrowserController::IsWebApp(browser))
    return false;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return false;

  LocationBarView* location_bar = browser_view->GetLocationBarView();

  return !location_bar || !location_bar->IsDrawn() ||
         location_bar->GetWidget()->IsFullscreen();
}

// A permission request should be auto-ignored if a user interacts with the
// LocationBar. The only exception is the NTP page where the user needs to press
// on a microphone icon to get a permission request.
bool ShouldIgnorePermissionRequest(content::WebContents* web_contents,
                                   Browser* browser) {
  DCHECK(web_contents);
  DCHECK(browser);

  // In case of the NTP, `WebContents::GetVisibleURL()` is equal to
  // `chrome://newtab/`, but the `LocationBarView` will be empty.
  if (web_contents->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL)) {
    return false;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return false;

  LocationBarView* location_bar = browser_view->GetLocationBarView();
  return location_bar && location_bar->IsEditingOrEmpty();
}

bool ShouldBubbleStartOpen(permissions::PermissionPrompt::Delegate* delegate) {
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipGestureSensitive)) {
    std::vector<permissions::PermissionRequest*> requests =
        delegate->Requests();
    const bool has_gesture =
        std::any_of(requests.begin(), requests.end(),
                    [](permissions::PermissionRequest* request) {
                      return request->GetGestureType() ==
                             permissions::PermissionRequestGestureType::GESTURE;
                    });
    if (has_gesture)
      return true;
  }
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipRequestTypeSensitive)) {
    // Notifications and geolocation are targeted here because they are usually
    // not necessary for the website to function correctly, so they can safely
    // be given less prominence.
    std::vector<permissions::PermissionRequest*> requests =
        delegate->Requests();
    const bool is_geolocation_or_notifications = std::any_of(
        requests.begin(), requests.end(),
        [](permissions::PermissionRequest* request) {
          permissions::RequestType request_type = request->request_type();
          return request_type == permissions::RequestType::kNotifications ||
                 request_type == permissions::RequestType::kGeolocation;
        });
    if (!is_geolocation_or_notifications)
      return true;
  }
  return false;
}

}  // namespace

std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    DLOG(WARNING) << "Permission prompt suppressed because the WebContents is "
                     "not attached to any Browser window.";
    return nullptr;
  }

  if (delegate->ShouldDropCurrentRequestIfCannotShowQuietly() &&
      IsFullScreenMode(browser)) {
    return nullptr;
  }

  // Auto-ignore the permission request if a user is typing into location bar.
  if (ShouldIgnorePermissionRequest(web_contents, browser)) {
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
  if (web_app::AppBrowserController::IsWebApp(browser_)) {
    SelectPwaPrompt();
  } else if (delegate_->ShouldCurrentRequestUseQuietUI()) {
    SelectQuietPrompt();
  } else {
    SelectNormalPrompt();
  }
}

PermissionPromptImpl::~PermissionPromptImpl() {
  switch (prompt_style_) {
    case PermissionPromptStyle::kBubbleOnly:
      CleanUpPromptBubble();
      break;
    case PermissionPromptStyle::kChip:
    case PermissionPromptStyle::kQuietChip:
      FinalizeChip();
      break;
    case PermissionPromptStyle::kLocationBarRightIcon:
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
  const bool is_location_bar_drawn =
      lbv && lbv->IsDrawn() && !lbv->GetWidget()->IsFullscreen();
  switch (prompt_style_) {
    case PermissionPromptStyle::kBubbleOnly:
      DCHECK(!lbv->IsChipActive());
      // TODO(crbug.com/1175231): Investigate why prompt_bubble_ can be null
      // here. Early return is preventing the crash from happening but we still
      // don't know the reason why it is null here and cannot reproduce it.
      if (!prompt_bubble_)
        return;

      // If |browser_| changed, recreate bubble for correct browser.
      if (was_browser_changed) {
        CleanUpPromptBubble();
        ShowBubble();
      } else {
        prompt_bubble_->UpdateAnchorPosition();
      }
      break;
    case PermissionPromptStyle::kChip:
    case PermissionPromptStyle::kQuietChip:
      DCHECK(!prompt_bubble_);
      DCHECK(lbv->IsChipActive());

      if (!is_location_bar_drawn) {
        FinalizeChip();
        ShowBubble();
      }
      break;
    case PermissionPromptStyle::kLocationBarRightIcon:
      break;
  }
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
      return ShouldBubbleStartOpen(delegate_)
                 ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP;
    case PermissionPromptStyle::kQuietChip:
      return permissions::PermissionUiSelector::ShouldSuppressAnimation(
                 delegate_->ReasonForUsingQuietUi())
                 ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP
                 : permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_QUIET_CHIP;
    case PermissionPromptStyle::kLocationBarRightIcon: {
      return permissions::PermissionUiSelector::ShouldSuppressAnimation(
                 delegate_->ReasonForUsingQuietUi())
                 ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_RIGHT_STATIC_ICON
                 : permissions::PermissionPromptDisposition::
                       LOCATION_BAR_RIGHT_ANIMATED_ICON;
    }
  }
}

void PermissionPromptImpl::CleanUpPromptBubble() {
  if (prompt_bubble_) {
    views::Widget* widget = prompt_bubble_->GetWidget();
    widget->RemoveObserver(this);
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    prompt_bubble_ = nullptr;
  }
}

views::Widget* PermissionPromptImpl::GetPromptBubbleWidgetForTesting() {
  if (prompt_bubble_) {
    return prompt_bubble_->GetWidget();
  }

  LocationBarView* lbv = GetLocationBarView();

  return lbv->chip()
             ? lbv->chip()->GetPromptBubbleWidgetForTesting()  // IN-TEST
             : nullptr;
}

void PermissionPromptImpl::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  prompt_bubble_ = nullptr;
}

bool PermissionPromptImpl::IsLocationBarDisplayed() {
  LocationBarView* lbv = GetLocationBarView();
  return lbv && lbv->IsDrawn() && !lbv->GetWidget()->IsFullscreen();
}

void PermissionPromptImpl::SelectPwaPrompt() {
  if (delegate_->ShouldCurrentRequestUseQuietUI()) {
    ShowQuietIcon();
  } else {
    ShowBubble();
  }
}

void PermissionPromptImpl::SelectNormalPrompt() {
  DCHECK(!delegate_->ShouldCurrentRequestUseQuietUI());
  if (ShouldCurrentRequestUseChip() && IsLocationBarDisplayed()) {
    ShowChip();
  } else {
    ShowBubble();
  }
}

void PermissionPromptImpl::SelectQuietPrompt() {
  if (ShouldCurrentRequestUseQuietChip()) {
    if (IsLocationBarDisplayed()) {
      ShowChip();
    } else {
      // If LocationBar is not displayed (Fullscreen mode), display a default
      // bubble only for non-abusive origins.
      DCHECK(!delegate_->ShouldDropCurrentRequestIfCannotShowQuietly());
      ShowBubble();
    }
  } else {
    ShowQuietIcon();
  }
}

LocationBarView* PermissionPromptImpl::GetLocationBarView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  return browser_view ? browser_view->GetLocationBarView() : nullptr;
}

void PermissionPromptImpl::ShowQuietIcon() {
  prompt_style_ = PermissionPromptStyle::kLocationBarRightIcon;
  // Shows the prompt as an indicator in the right side of the omnibox.
  content_settings::UpdateLocationBarUiForWebContents(web_contents_);
}

void PermissionPromptImpl::ShowBubble() {
  prompt_style_ = PermissionPromptStyle::kBubbleOnly;
  prompt_bubble_ =
      new PermissionPromptBubbleView(browser_, delegate_->GetWeakPtr(),
                                     permission_requested_time_, prompt_style_);
  prompt_bubble_->Show();
  prompt_bubble_->GetWidget()->AddObserver(this);
}

void PermissionPromptImpl::ShowChip() {
  LocationBarView* lbv = GetLocationBarView();
  DCHECK(lbv);

  if (delegate_->ShouldCurrentRequestUseQuietUI()) {
    lbv->DisplayQuietChip(
        delegate_, !permissions::PermissionUiSelector::ShouldSuppressAnimation(
                       delegate_->ReasonForUsingQuietUi()));
    prompt_style_ = PermissionPromptStyle::kQuietChip;
  } else {
    lbv->DisplayChip(delegate_, ShouldBubbleStartOpen(delegate_));
    prompt_style_ = PermissionPromptStyle::kChip;
  }
}

bool PermissionPromptImpl::ShouldCurrentRequestUseChip() {
  if (!base::FeatureList::IsEnabled(permissions::features::kPermissionChip))
    return false;

  std::vector<permissions::PermissionRequest*> requests = delegate_->Requests();
  return std::all_of(requests.begin(), requests.end(),
                     [](permissions::PermissionRequest* request) {
                       return request->GetRequestChipText().has_value();
                     });
}

bool PermissionPromptImpl::ShouldCurrentRequestUseQuietChip() {
  if (!base::FeatureList::IsEnabled(
          permissions::features::kPermissionQuietChip)) {
    return false;
  }

  std::vector<permissions::PermissionRequest*> requests = delegate_->Requests();
  return std::all_of(requests.begin(), requests.end(),
                     [](permissions::PermissionRequest* request) {
                       return request->request_type() ==
                                  permissions::RequestType::kNotifications ||
                              request->request_type() ==
                                  permissions::RequestType::kGeolocation;
                     });
}

void PermissionPromptImpl::FinalizeChip() {
  LocationBarView* lbv = GetLocationBarView();
  if (lbv && lbv->chip()) {
    lbv->FinalizeChip();
  }
}
