// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_FOCUS_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_FOCUS_IPH_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Controls the display of In-Product Help (IPH) for the Tab Groups Focusing
// feature.
//
// Observes the TabStripModel and triggers the discovery promo when:
// 1. At least 1 tab group exists in the tab strip.
// 2. Total tab count exceeds `features::kTabGroupsFocusingIphMinTabs`.
class TabGroupFocusIphController : public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(TabGroupFocusIphController);
  explicit TabGroupFocusIphController(BrowserWindowInterface* interface);
  ~TabGroupFocusIphController() override;

  TabGroupFocusIphController(const TabGroupFocusIphController&) = delete;
  TabGroupFocusIphController& operator=(const TabGroupFocusIphController&) =
      delete;

  static TabGroupFocusIphController* From(BrowserWindowInterface* interface);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupAdded(const tab_groups::TabGroupId& group_id) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;

 private:
  void ScheduleMaybeShowPromo();
  void MaybeShowPromo();

  base::OneShotTimer promo_timer_;
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  ui::ScopedUnownedUserData<TabGroupFocusIphController> scoped_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_FOCUS_IPH_CONTROLLER_H_
