// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_location_bar.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

WebUILocationBar::WebUILocationBar(BrowserWindowInterface* browser)
    : LocationBar(browser->GetBrowserForMigrationOnly()->command_controller()),
      browser_(browser) {}

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

content::WebContents* WebUILocationBar::GetWebContents() {
  NOTIMPLEMENTED();
  return nullptr;
}

LocationBarModel* WebUILocationBar::GetLocationBarModel() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUILocationBar::OnChanged() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::OnPopupVisibilityChanged() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateWithoutTabRestore() {
  NOTIMPLEMENTED();
}

LocationBarTesting* WebUILocationBar::GetLocationBarForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}
