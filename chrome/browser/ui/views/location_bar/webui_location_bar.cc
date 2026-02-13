// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "ui/views/bubble/bubble_border.h"

WebUILocationBar::WebUILocationBar(
    chrome::BrowserCommandController* command_controller,
    WebUIToolbarWebView* toolbar_view)
    : LocationBar(command_controller), toolbar_view_(toolbar_view) {}

WebUILocationBar::~WebUILocationBar() = default;

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
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebUILocationBar::ShouldCloseOmniboxPopup(ui::MouseEvent* event) {
  NOTIMPLEMENTED();
  return false;
}

ChipController* WebUILocationBar::GetChipController() {
  NOTIMPLEMENTED();
  return nullptr;
}

content::WebContents* WebUILocationBar::GetWebContents() {
  NOTIMPLEMENTED();
  return nullptr;
}

LocationBarModel* WebUILocationBar::GetLocationBarModel() {
  NOTIMPLEMENTED();
  return nullptr;
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
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUILocationBar::OnChanged() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateWithoutTabRestore() {
  NOTIMPLEMENTED();
}

bool WebUILocationBar::IsVisible() const {
  NOTIMPLEMENTED();
  return true;
}

bool WebUILocationBar::IsDrawn() const {
  NOTIMPLEMENTED();
  return true;
}

bool WebUILocationBar::IsTopLevelFullscreen() const {
  NOTIMPLEMENTED();
  return false;
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
  NOTIMPLEMENTED();
  return gfx::Size();
}

gfx::Size WebUILocationBar::PreferredSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
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
