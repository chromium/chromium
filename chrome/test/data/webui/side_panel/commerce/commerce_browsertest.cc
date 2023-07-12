// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_test.h"

class SidePanelShoppingInsightsTest : public WebUIMochaBrowserTest {
 protected:
  SidePanelShoppingInsightsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        commerce::kPriceInsights,
        {
            {commerce::kPriceInsightsShowFeedbackParam, "true"},
        });
    set_test_loader_host(commerce::kChromeUIShoppingInsightsSidePanelHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelShoppingInsightsTest, App) {
  RunTest("side_panel/commerce/shopping_insights_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelShoppingInsightsTest, PriceTrackingSection) {
  RunTest("side_panel/commerce/price_tracking_section_test.js", "mocha.run()");
}
