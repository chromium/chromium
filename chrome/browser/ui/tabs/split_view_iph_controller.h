// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_VIEW_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_VIEW_IPH_CONTROLLER_H_

#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserView;

namespace ui {
class TrackedElement;
}  // namespace ui

// Provides the logic to show the IPH promo for the split view feature.
//
// Current IPH logic has two scenarios:
//
// 1. When a user creates a split view two or more times, show an IPH promo
//    that asks the user if they want to pin the Split View button.
// 2. When a user switches between the same two tabs three or more times, show
//    an IPH tutorial that guides the user to add a tab to a split view.
class SplitViewIphController : public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(SplitViewIphController);
  explicit SplitViewIphController(BrowserWindowInterface* interface);
  ~SplitViewIphController() override;

  SplitViewIphController(const SplitViewIphController&) = delete;
  SplitViewIphController& operator=(const SplitViewIphController&) = delete;

  static SplitViewIphController* From(BrowserWindowInterface* interface);

  // Returns the most recently inactive tab
  ui::TrackedElement* GetTabSwitchIPHAnchor(BrowserView* browser_view);

  int get_recent_tabs_size() const { return recent_tabs_.size(); }
  int get_tab_switch_count() const { return tab_switch_count_; }

  // TabStripModelObserver
  void OnSplitTabChanged(const SplitTabChange& change) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  static constexpr int kNumTabsTracked = 2;
  static constexpr int kMostRecentTabTrackerIndex = 0;
  static constexpr int kLeastRecentTabTrackerIndex = 1;

  void MaybeShowPromo(const base::Feature& feature);

  // Add new tab to tracked tabs while removing the least recently used tab
  void AddNewTabToTracker(tabs::TabInterface* new_tab);

  // Remove tabs from tracked tabs
  void RemoveTabFromTracker(const TabStripModelChange::Remove* remove_contents);

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Number of times user has switched between the same two tabs.
  int tab_switch_count_ = 0;

  // Two most recently active tabs. Front is current active tab, end is previous
  // active tab. When a third tab becomes active, push to front, which will
  // evict the oldest from the end.
  base::circular_deque<tabs::TabInterface*> recent_tabs_;

  ui::ScopedUnownedUserData<SplitViewIphController> scoped_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_VIEW_IPH_CONTROLLER_H_
