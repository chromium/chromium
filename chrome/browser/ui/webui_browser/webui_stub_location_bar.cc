// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_stub_location_bar.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "ui/views/bubble/bubble_border.h"

WebUIStubLocationBar::WebUIStubLocationBar(WebUIBrowserWindow* window)
    : LocationBar(window->browser()
                      ->GetBrowserForMigrationOnly()
                      ->command_controller()),
      window_(window) {}

WebUIStubLocationBar::~WebUIStubLocationBar() = default;

void WebUIStubLocationBar::FocusLocation(bool is_user_initiated,
                                         bool clear_focus_if_failed) {
  NOTIMPLEMENTED();
}

void WebUIStubLocationBar::FocusSearch() {
  NOTIMPLEMENTED();
}

void WebUIStubLocationBar::UpdateFocusBehavior(bool toolbar_visible) {
  NOTIMPLEMENTED();
}

void WebUIStubLocationBar::UpdateContentSettingsIcons() {
  NOTIMPLEMENTED();
}

void WebUIStubLocationBar::SaveStateToContents(content::WebContents* contents) {
  NOTIMPLEMENTED();
}

void WebUIStubLocationBar::Revert() {
  NOTIMPLEMENTED();
}

OmniboxView* WebUIStubLocationBar::GetOmniboxView() {
  NOTIMPLEMENTED();
  return nullptr;
}

OmniboxController* WebUIStubLocationBar::GetOmniboxController() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebUIStubLocationBar::ShouldCloseOmniboxPopup(ui::MouseEvent* event) {
  NOTIMPLEMENTED();
  return false;
}

ChipController* WebUIStubLocationBar::GetChipController() {
  NOTIMPLEMENTED();
  return nullptr;
}

content::WebContents* WebUIStubLocationBar::GetWebContents() {
  NOTIMPLEMENTED();
  return nullptr;
}

LocationBarModel* WebUIStubLocationBar::GetLocationBarModel() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<bubble_anchor_util::AnchorConfiguration>
WebUIStubLocationBar::GetChipAnchor() {
  ui::TrackedElement* location_button =
      BrowserElements::From(window_->browser())
          ->GetElement(kLocationIconElementId);
  CHECK(location_button) << "Location button not found";
  return {{location_button, nullptr, views::BubbleBorder::TOP_LEFT}};
}

ui::TrackedElement* WebUIStubLocationBar::GetAnchorOrNull() {
  NOTIMPLEMENTED();
  return nullptr;
}

Browser* WebUIStubLocationBar::GetBrowser() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIStubLocationBar::OnChanged() {
  NOTIMPLEMENTED();
}

void WebUIStubLocationBar::UpdateWithoutTabRestore() {
  NOTIMPLEMENTED();
}

bool WebUIStubLocationBar::IsInitialized() const {
  NOTIMPLEMENTED();
  return true;
}

bool WebUIStubLocationBar::IsVisible() const {
  NOTIMPLEMENTED();
  return true;
}

bool WebUIStubLocationBar::IsDrawn() const {
  NOTIMPLEMENTED();
  return true;
}

bool WebUIStubLocationBar::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIStubLocationBar::IsEditingOrEmpty() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIStubLocationBar::InvalidateLayout() {
  NOTIMPLEMENTED();
}

gfx::Rect WebUIStubLocationBar::Bounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Size WebUIStubLocationBar::MinimumSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

gfx::Size WebUIStubLocationBar::PreferredSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

void WebUIStubLocationBar::Update(content::WebContents* contents) {
  NOTIMPLEMENTED();
}

void WebUIStubLocationBar::ResetTabState(content::WebContents* contents) {
  NOTIMPLEMENTED();
}

bool WebUIStubLocationBar::HasSecurityStateChanged() {
  NOTIMPLEMENTED();
  return false;
}

LocationBarTesting* WebUIStubLocationBar::GetLocationBarForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}
