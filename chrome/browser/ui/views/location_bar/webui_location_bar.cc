// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "ui/base/interaction/element_events.h"
#include "ui/views/bubble/bubble_border.h"

WebUILocationBar::WebUILocationBar(Browser* browser,
                                   LocationBarView::Delegate* delegate)
    : LocationBar(browser->command_controller()),
      browser_(browser),
      delegate_(delegate) {}

WebUILocationBar::~WebUILocationBar() = default;

void WebUILocationBar::Init(WebUIToolbarWebView* toolbar_view) {
  toolbar_view_ = toolbar_view;

  // TODO(crbug.com/474060773): Replace the View with a WebUI impl.
  permission_dashboard_view_ =
      toolbar_view->AddChildView(std::make_unique<PermissionDashboardView>());

  permission_dashboard_controller_ =
      std::make_unique<PermissionDashboardController>(
          /*location_bar=*/this,
          /*content_settings_image_delegate=*/this, permission_dashboard_view_);

  omnibox_controller_ =
      std::make_unique<OmniboxController>(std::make_unique<ChromeOmniboxClient>(
          /*location_bar=*/this, browser_, browser_->profile()));
  omnibox_view_ =
      std::make_unique<WebUIReadOnlyOmnibox>(omnibox_controller_.get(), this);

  // Unretained is safe because `this` owns `moved_subscription_`.
  moved_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          ui::kElementBoundsChangedEvent, kLocationBarElementId,
          BrowserElements::From(browser_)->GetContext(),
          base::BindRepeating(&WebUILocationBar::OnMoved,
                              base::Unretained(this)));

  is_initialized_ = true;
}

void WebUILocationBar::FocusLocation(bool is_user_initiated,
                                     bool clear_focus_if_failed) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::FocusSearch() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateFocusBehavior(bool toolbar_visible) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateContentSettingsIcons() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::SaveStateToContents(content::WebContents* contents) {
  omnibox_view_->SaveStateToTab(contents);
}

void WebUILocationBar::Revert() {
  omnibox_view_->RevertAll();
}

OmniboxView* WebUILocationBar::GetOmniboxView() {
  return omnibox_view_.get();
}

OmniboxController* WebUILocationBar::GetOmniboxController() {
  return omnibox_controller_.get();
}

bool WebUILocationBar::ShouldCloseOmniboxPopup(ui::MouseEvent* event) {
  NOTIMPLEMENTED();
  return false;
}

ChipController* WebUILocationBar::GetChipController() {
  return permission_dashboard_controller_->request_chip_controller();
}

content::WebContents* WebUILocationBar::GetWebContents() {
  return delegate_->GetWebContents();
}

LocationBarModel* WebUILocationBar::GetLocationBarModel() {
  return delegate_->GetLocationBarModel();
}

std::optional<bubble_anchor_util::AnchorConfiguration>
WebUILocationBar::GetChipAnchor() {
  NOTIMPLEMENTED();
  return {{nullptr, std::nullopt, views::BubbleBorder::TOP_LEFT}};
}

ui::TrackedElement* WebUILocationBar::GetAnchorOrNull() {
  return BrowserElements::From(browser_)->GetElement(kLocationBarElementId);
}

Browser* WebUILocationBar::GetBrowser() {
  return browser_.get();
}

Profile* WebUILocationBar::GetProfile() {
  return browser_->profile();
}

void WebUILocationBar::OnChanged() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateWithoutTabRestore() {
  Update(nullptr);
}

bool WebUILocationBar::IsInitialized() const {
  return is_initialized_;
}

bool WebUILocationBar::IsVisible() const {
  return toolbar_view_ && toolbar_view_->GetVisible();
}

bool WebUILocationBar::IsDrawn() const {
  return toolbar_view_ && toolbar_view_->IsDrawn();
}

bool WebUILocationBar::IsFullscreen() const {
  return toolbar_view_ && toolbar_view_->GetWidget()->IsFullscreen();
}

bool WebUILocationBar::IsEditingOrEmpty() const {
  return omnibox_view_->IsEditingOrEmpty();
}

void WebUILocationBar::InvalidateLayout() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebUILocationBar::OnChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

gfx::Rect WebUILocationBar::Bounds() const {
  gfx::Rect screen_rect = BoundsInScreen();
  if (!screen_rect.IsEmpty()) {
    return views::View::ConvertRectFromScreen(toolbar_view_, screen_rect);
  }
  return gfx::Rect();
}

gfx::Rect WebUILocationBar::BoundsInScreen() const {
  ui::TrackedElement* anchor =
      BrowserElements::From(browser_)->GetElement(kLocationBarElementId);
  return anchor ? anchor->GetScreenBounds() : gfx::Rect();
}

gfx::Size WebUILocationBar::MinimumSize() const {
  // TODO(crbug.com/474060468): Proper calculation.
  return gfx::Size(400, 34);
}

gfx::Size WebUILocationBar::PreferredSize() const {
  // TODO(crbug.com/474060468): Proper calculation.
  return gfx::Size(400, 34);
}

void WebUILocationBar::Update(content::WebContents* contents) {
  NOTIMPLEMENTED();  // Or rather needs a bunch more

  if (contents) {
    omnibox_view_->OnTabChanged(contents);
  } else {
    omnibox_view_->Update();
  }

  OnChanged();
}

void WebUILocationBar::ResetTabState(content::WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

bool WebUILocationBar::HasSecurityStateChanged() {
  NOTIMPLEMENTED();
  return false;
}

LocationBarTesting* WebUILocationBar::GetLocationBarForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebUILocationBar::ShouldHideContentSettingImage() {
  NOTIMPLEMENTED();
  return false;
}

content::WebContents* WebUILocationBar::GetContentSettingWebContents() {
  NOTIMPLEMENTED();
  return nullptr;
}

ContentSettingBubbleModelDelegate*
WebUILocationBar::GetContentSettingBubbleModelDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUILocationBar::OnMoved(ui::TrackedElement*) {
  NotifyBoundsChanged();
}
