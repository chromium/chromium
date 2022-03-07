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
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_frame_view.h"

namespace {

bool IsFullScreenMode(content::WebContents* web_contents, Browser* browser) {
  DCHECK(web_contents);
  DCHECK(browser);

  // PWA uses the title bar as a substitute for LocationBarView.
  if (web_app::AppBrowserController::IsWebApp(browser))
    return false;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return false;

  LocationBarView* location_bar = browser_view->GetLocationBarView();

  return !location_bar || !location_bar->IsDrawn();
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
      IsFullScreenMode(web_contents, browser)) {
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
      DCHECK(!chip_);
      if (prompt_bubble_)
        prompt_bubble_->GetWidget()->Close();
      break;
    case PermissionPromptStyle::kChip:
    case PermissionPromptStyle::kQuietChip:
      DCHECK(!prompt_bubble_);
      DCHECK(chip_);
      FinalizeChip();
      break;
    case PermissionPromptStyle::kLocationBarRightIcon:
      DCHECK(!prompt_bubble_);
      DCHECK(!chip_);
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
      DCHECK(!chip_);
      // TODO(crbug.com/1175231): Investigate why prompt_bubble_ can be null
      // here. Early return is preventing the crash from happening but we still
      // don't know the reason why it is null here and cannot reproduce it.
      if (!prompt_bubble_)
        return;

      if (ShouldCurrentRequestUseChip() && is_location_bar_drawn) {
        // Change prompt style to chip to avoid dismissing request while
        // switching UI style.
        prompt_bubble_->SetPromptStyle(PermissionPromptStyle::kChip);
        prompt_bubble_->GetWidget()->Close();
        ShowChip();
        chip_->OpenBubble();
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

      if (!lbv->chip()) {
        chip_ = lbv->DisplayChip(delegate_, ShouldBubbleStartOpen(delegate_));
      }
      // If there is fresh pending request shown as chip UI and location bar
      // isn't visible anymore, show bubble UI instead.
      if (!chip_->is_fully_collapsed() && !is_location_bar_drawn) {
        FinalizeChip();
        ShowBubble();
      }
      break;
    case PermissionPromptStyle::kQuietChip:
      DCHECK(!prompt_bubble_);

      if (!lbv->chip()) {
        chip_ = lbv->DisplayQuietChip(
            delegate_,
            !permissions::PermissionUiSelector::ShouldSuppressAnimation(
                delegate_->ReasonForUsingQuietUi()));
      }
      // If there is fresh pending request shown as chip UI and location bar
      // isn't visible anymore, show bubble UI instead.
      if (!chip_->is_fully_collapsed() && !is_location_bar_drawn) {
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

views::Widget* PermissionPromptImpl::GetPromptBubbleWidgetForTesting() {
  if (prompt_bubble_) {
    return prompt_bubble_->GetWidget();
  }
  return chip_ ? chip_->GetPromptBubbleWidgetForTesting()  // IN-TEST
               : nullptr;
}

void PermissionPromptImpl::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, prompt_bubble_->GetWidget());
  widget->RemoveObserver(this);
  prompt_bubble_ = nullptr;
}

bool PermissionPromptImpl::IsLocationBarDisplayed() {
  LocationBarView* lbv = GetLocationBarView();
  return lbv && lbv->IsDrawn();
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
  if (ShouldCurrentRequestUseChip()) {
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
  prompt_bubble_ = new PermissionPromptBubbleView(
      browser_, delegate_, permission_requested_time_, prompt_style_);
  prompt_bubble_->Show();
  prompt_bubble_->GetWidget()->AddObserver(this);
}

void PermissionPromptImpl::ShowChip() {
  LocationBarView* lbv = GetLocationBarView();
  DCHECK(lbv);

  if (delegate_->ShouldCurrentRequestUseQuietUI()) {
    chip_ = lbv->DisplayQuietChip(
        delegate_, !permissions::PermissionUiSelector::ShouldSuppressAnimation(
                       delegate_->ReasonForUsingQuietUi()));
    prompt_style_ = PermissionPromptStyle::kQuietChip;
  } else {
    chip_ = lbv->DisplayChip(delegate_, ShouldBubbleStartOpen(delegate_));
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
  GetLocationBarView()->FinalizeChip();
  chip_ = nullptr;
}
