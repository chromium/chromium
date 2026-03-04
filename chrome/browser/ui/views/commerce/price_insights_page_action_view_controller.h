// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_PAGE_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_PAGE_ACTION_VIEW_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class ScopedCallToActionLock;

namespace page_actions {
class PageActionController;
}

namespace tabs {
class TabInterface;
}

// Enum for logging the price insights icon label. Each label we ever use
// should have a separate enum even if they are semantically similar (e.g.
// "Price is low" vs. "Great price") since it could have a nontrivial effect
// on the click-through rate. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused.
enum class PriceInsightsIconLabelType {
  kNone = 0,
  kPriceIsLow = 1,
  kPriceIsHigh = 2,
  kMaxValue = kPriceIsHigh,
};

namespace commerce {

// This class is responsible for interacting with the PageActionController
// and CommerceUITabHelper to determine whether the Price Insights icon should
// be shown, hidden, or expanded with additional text based on the current
// page's commerce-related context.
class PriceInsightsPageActionViewController {
 public:
  DECLARE_USER_DATA(PriceInsightsPageActionViewController);

  PriceInsightsPageActionViewController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);

  PriceInsightsPageActionViewController(
      const PriceInsightsPageActionViewController&) = delete;
  PriceInsightsPageActionViewController& operator=(
      const PriceInsightsPageActionViewController&) = delete;

  ~PriceInsightsPageActionViewController();

  static PriceInsightsPageActionViewController* From(tabs::TabInterface& tab);

  // Updates the Price Insights page action icon based on the current tab state.
  // If the icon should be shown, it may also display an expanded label or a
  // suggestion chip depending on the context provided by the commerce UI tab
  // helper.
  void UpdatePageActionIcon(bool should_shown_icon,
                            bool should_expand_icon,
                            PriceInsightsIconLabelType label_type);

 private:
  // Reference to the tab interface, which provides access to tab-specific
  // features.
  const raw_ref<tabs::TabInterface> tab_interface_;

  // Unowned reference to the page action controller that will coordinate
  // requests from this object.
  const raw_ref<page_actions::PageActionController> page_action_controller_;

  std::unique_ptr<ScopedCallToActionLock> scoped_call_to_action_lock_;

  ui::ScopedUnownedUserData<PriceInsightsPageActionViewController>
      scoped_unowned_user_data_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_PAGE_ACTION_VIEW_CONTROLLER_H_
