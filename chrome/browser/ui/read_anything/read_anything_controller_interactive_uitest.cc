// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_controller.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/accelerators/accelerator.h"

class ReadAnythingLineFocusCUJTest : public InteractiveFeaturePromoTest {
 public:
  template <typename... Args>
  explicit ReadAnythingLineFocusCUJTest(Args&&... args)
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHReadingModeLineFocusFeature})) {}
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    distillable_url_ = embedded_test_server()->GetURL("/long_text_page.html");
    non_distillable_url_ = GURL("chrome://blank");

    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingLineFocus,
        feature_engagement::kIPHReadingModeLineFocusFeature};
    feature_list_.InitAndEnableFeatures(enabled_features);

    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

  GURL distillable_url_;
  GURL non_distillable_url_;
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingLineFocusCUJTest, ShowAndHideIph) {
  ui::Accelerator reading_mode_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SHOW_READING_MODE_KEYBOARD, &reading_mode_accelerator));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),

      // Use the keyboard shortcut command to open Reading Mode.
      SendAccelerator(kBrowserViewElementId, reading_mode_accelerator),

      // Wait for the promo to show.
      WaitForPromo(feature_engagement::kIPHReadingModeLineFocusFeature),

      // Hide the Iph by navigating to an empty page.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}
