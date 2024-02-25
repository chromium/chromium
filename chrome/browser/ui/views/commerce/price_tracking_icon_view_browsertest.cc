// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
const char kTestURL[] = "about:blank";
}  // namespace

class PriceTrackingIconViewBrowserTest : public UiBrowserTest {
 public:
  PriceTrackingIconViewBrowserTest() {
    test_features_.InitWithFeatures({commerce::kShoppingList},
                                    {commerce::kPriceInsights});
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* icon_view = GetChip();
    if (name == "forced_show_tracking_price") {
      SimulateServerPriceTrackState(true);
      icon_view->ForceVisibleForTesting(/*is_tracking_price=*/true);
    } else {
      CHECK_EQ(name, "forced_show_track_price");
      SimulateServerPriceTrackState(false);
      icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);
    }
  }

  bool VerifyUi() override {
    auto* price_tracking_chip = GetChip();
    if (!price_tracking_chip) {
      return false;
    }

    // TODO(meiliang): call VerifyPixelUi here after PriceTrackingIconView is
    // finished implementing.
    return true;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
  base::test::ScopedFeatureList test_features_;

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBarView* GetLocationBarView() {
    return GetBrowserView()->toolbar()->location_bar();
  }

  PriceTrackingIconView* GetChip() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(GetLocationBarView());
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kPriceTrackingChipElementId, context);

    return matched_view
               ? views::AsViewClass<PriceTrackingIconView>(matched_view)
               : nullptr;
  }

  void SimulateServerPriceTrackState(bool is_price_tracked) {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());

    commerce::AddProductBookmark(bookmark_model, u"title", GURL(kTestURL), 0,
                                 is_price_tracked);
  }
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewBrowserTest,
                       InvokeUi_forced_show_tracking_price) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewBrowserTest,
                       InvokeUi_forced_show_track_price) {
  ShowAndVerifyUi();
}
