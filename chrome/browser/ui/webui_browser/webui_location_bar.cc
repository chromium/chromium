// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_location_bar.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/element_tracker_views.h"

WebUILocationBar::WebUILocationBar(WebUIBrowserWindow* window)
    : LocationBar(window->browser()
                      ->GetBrowserForMigrationOnly()
                      ->command_controller()),
      window_(window) {}

WebUILocationBar::~WebUILocationBar() = default;

void WebUILocationBar::FocusLocation(bool is_user_initiated) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::FocusSearch() {
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
  ui::TrackedElement* location_button =
      BrowserElements::From(window_->browser())
          ->GetElement(kLocationIconElementId);
  CHECK(location_button) << "Location button not found";
  return {{location_button, nullptr, views::BubbleBorder::TOP_LEFT}};
}

void WebUILocationBar::OnChanged() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateWithoutTabRestore() {
  NOTIMPLEMENTED();
}

LocationBarTesting* WebUILocationBar::GetLocationBarForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}
