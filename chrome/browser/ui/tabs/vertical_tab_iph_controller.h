// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_IPH_CONTROLLER_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

namespace views {
class View;
}  // namespace views

// Controls the display of In-Product Help (IPH) for the Vertical Tabs feature.
//
// This controller observes the TabStripModel and triggers a promo when the
// horizontal tabstrip becomes "crowded." Criteria for the promo include:
// 1. The user has not used Vertical Tabs before (tracked via prefs).
// 2. The tab count exceeds a minimum threshold.
// 3. The physical width of unpinned tabs has shrunk significantly
//    compared to their ideal width.
class VerticalTabIphController : public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(VerticalTabIphController);
  explicit VerticalTabIphController(BrowserWindowInterface* interface);
  ~VerticalTabIphController() override;

  VerticalTabIphController(const VerticalTabIphController&) = delete;
  VerticalTabIphController& operator=(const VerticalTabIphController&) = delete;

  static VerticalTabIphController* From(BrowserWindowInterface* interface);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  void MaybeShowPromo();
  bool IsTabShrunk(views::View* tab);

  base::OneShotTimer promo_timer_;
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  ui::ScopedUnownedUserData<VerticalTabIphController> scoped_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_IPH_CONTROLLER_H_
