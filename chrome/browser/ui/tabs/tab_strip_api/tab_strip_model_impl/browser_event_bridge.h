// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_BROWSER_EVENT_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_BROWSER_EVENT_BRIDGE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"

class BrowserWindowInterface;
enum class BrowserThemeChangeType;

namespace tabs_api::tab_strip_model {

class TabStripModelEventBridge;

// Bridges browser-window level events into the tab strip event stream. This
// mirrors TabStripModelEventBridge but observes the browser window rather than
// the TabStripModel.
class BrowserEventBridge {
 public:
  BrowserEventBridge(BrowserWindowInterface& browser_window_interface,
                     TabStripModelEventBridge& event_bridge);
  BrowserEventBridge(const BrowserEventBridge&) = delete;
  BrowserEventBridge& operator=(const BrowserEventBridge&) = delete;
  ~BrowserEventBridge();

 private:
  // Favicons are rasterized against the active color provider, so they must be
  // re-emitted when the theme changes.
  void OnBrowserThemeChanged(BrowserThemeChangeType change_type);

  raw_ref<TabStripModelEventBridge> event_bridge_;
  base::CallbackListSubscription theme_changed_subscription_;
};

}  // namespace tabs_api::tab_strip_model

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_BROWSER_EVENT_BRIDGE_H_
