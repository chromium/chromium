// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/browser_event_bridge.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_theme_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tabs_api::tab_strip_model {

BrowserEventBridge::BrowserEventBridge(
    BrowserWindowInterface& browser_window_interface,
    TabStripModelEventBridge& event_bridge)
    : event_bridge_(event_bridge) {
  auto* const theme_observer =
      BrowserWindowThemeObserver::From(&browser_window_interface);
  CHECK(theme_observer);
  theme_changed_subscription_ =
      theme_observer->RegisterThemeChangedCallback(base::BindRepeating(
          &BrowserEventBridge::OnBrowserThemeChanged, base::Unretained(this)));
}

BrowserEventBridge::~BrowserEventBridge() = default;

void BrowserEventBridge::OnBrowserThemeChanged(
    BrowserThemeChangeType change_type) {
  event_bridge_->NotifyFaviconsChanged();
}

}  // namespace tabs_api::tab_strip_model
