// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_PAGE_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_PAGE_ACTION_VIEW_CONTROLLER_H_

#include "base/memory/raw_ref.h"

class ScopedWindowCallToAction;

namespace tabs {
class TabInterface;
}

namespace commerce {

// This class is responsible for interacting with the PageActionController
// and CommerceUITabHelper to determine whether the Price Insights icon should
// be shown, hidden, or expanded with additional text based on the current
// page's commerce-related context.
class PriceInsightsPageActionViewController {
 public:
  explicit PriceInsightsPageActionViewController(
      tabs::TabInterface& tab_interface);
  PriceInsightsPageActionViewController(
      const PriceInsightsPageActionViewController&) = delete;
  PriceInsightsPageActionViewController& operator=(
      const PriceInsightsPageActionViewController&) = delete;

  ~PriceInsightsPageActionViewController();

  // Updates the Price Insights page action icon based on the current tab state.
  // If the icon should be shown, it may also display an expanded label or a
  // suggestion chip depending on the context provided by the commerce UI tab
  // helper.
  void UpdatePageActionIcon();

 private:
  // Reference to the tab interface, which provides access to tab-specific
  // features.
  const raw_ref<tabs::TabInterface> tab_interface_;

  std::unique_ptr<ScopedWindowCallToAction> scoped_window_call_to_action_ptr_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_PAGE_ACTION_VIEW_CONTROLLER_H_
