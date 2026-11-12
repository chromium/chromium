// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_SCOPED_TEST_TAB_STRIP_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_SCOPED_TEST_TAB_STRIP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip_event_bridge.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace tabs_api::testing {

// Registers a `TabStripServiceFeature` backed by an in-memory `ToyTabStrip` on
// `host` for unit tests. Automatically observes added tabs' `WebContents` (when
// present) so navigation, loading, and title changes propagate to
// `TabStripServiceView` and `TabStripApiObserver`s.
class ScopedTestTabStrip {
 public:
  explicit ScopedTestTabStrip(ui::UnownedUserDataHost& host);
  ScopedTestTabStrip(const ScopedTestTabStrip&) = delete;
  ScopedTestTabStrip& operator=(const ScopedTestTabStrip&) = delete;
  ~ScopedTestTabStrip();

  // Adds `tab` to the tab strip and notifies observers.
  void AddTab(tabs::TabInterface* tab, bool active = true);

  // Adds a synthetic tab without a `tabs::TabInterface` and returns its handle.
  tabs::TabHandle AddTab(const GURL& url,
                         std::u16string title = u"",
                         bool active = true);

  // Marks `tab` / `handle` as the active tab and notifies observers.
  void SetActiveTab(tabs::TabInterface* tab);
  void SetActiveTab(tabs::TabHandle handle);

  // Re-reads `tab`'s current state (`title`, `url`, `network_state`,
  // `last_active`) and broadcasts an `OnDataChangedEvent`. Useful when tests
  // mutate `TestWebContents` directly without firing `WebContentsObserver`
  // callbacks (e.g. `WebContentsTester::SetTitle()`).
  void UpdateTab(tabs::TabInterface* tab);

  // Removes `handle` / `tab` from the tab strip and broadcasts
  // `OnNodesClosedEvent`.
  void CloseTab(tabs::TabHandle handle);
  void CloseTab(tabs::TabInterface* tab);

  ToyTabStrip& toy_tab_strip() { return toy_tab_strip_; }
  ToyTabStripEventBridge& event_bridge() { return *event_bridge_; }
  TabStripServiceFeature& feature() { return *feature_; }

 private:
  class TabContentsObserver;

  void NotifyTabCreated(const Tab& tab, size_t index);

  ToyTabStrip toy_tab_strip_;
  raw_ptr<ToyTabStripEventBridge> event_bridge_ = nullptr;
  std::unique_ptr<TabStripServiceFeature> feature_;
  std::vector<std::unique_ptr<TabContentsObserver>> tab_observers_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_SCOPED_TEST_TAB_STRIP_H_
