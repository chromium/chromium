// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
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
  NOTIMPLEMENTED();
}

void WebUILocationBar::Revert() {
  NOTIMPLEMENTED();
}

OmniboxView* WebUILocationBar::GetOmniboxView() {
  NOTIMPLEMENTED();
  return nullptr;
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
  return {{nullptr, nullptr, views::BubbleBorder::TOP_LEFT}};
}

ui::TrackedElement* WebUILocationBar::GetAnchorOrNull() {
  NOTIMPLEMENTED();
  return nullptr;
}

Browser* WebUILocationBar::GetBrowser() {
  return browser_.get();
}

void WebUILocationBar::OnChanged() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateWithoutTabRestore() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return false;
}

void WebUILocationBar::InvalidateLayout() {
  NOTIMPLEMENTED();
}

gfx::Rect WebUILocationBar::Bounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
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
  NOTIMPLEMENTED();
}

void WebUILocationBar::ResetTabState(content::WebContents* contents) {
  NOTIMPLEMENTED();
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
