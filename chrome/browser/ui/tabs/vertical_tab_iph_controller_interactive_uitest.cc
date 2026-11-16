// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

class VerticalTabIphControllerInteractiveUiTest
    : public InteractiveFeaturePromoTest {
 public:
  VerticalTabIphControllerInteractiveUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromosWithParams(
            {{feature_engagement::kIPHVerticalTabstripTutorialFeature,
              {{"x_min_tabs_for_promo", "2"}}}})) {}
  ~VerticalTabIphControllerInteractiveUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(VerticalTabIphControllerInteractiveUiTest,
                       ShowsPromoWithoutShrinkageWhenMinTabsBelow20) {
  RunTestSequence(
      Do([this]() {
        chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
        chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
      }),
      WaitForPromo(feature_engagement::kIPHVerticalTabstripTutorialFeature));
}
