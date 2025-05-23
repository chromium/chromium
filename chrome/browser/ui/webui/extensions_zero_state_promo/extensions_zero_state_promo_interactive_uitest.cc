// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"
#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);
}  // namespace

class ExtensionsZeroStatePromoTestBase : public InteractiveFeaturePromoTest {
 public:
  ExtensionsZeroStatePromoTestBase(
      feature_engagement::IPHExtensionsZeroStatePromoVariant iphVariant)
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromosWithParams(
            {{feature_engagement::kIPHExtensionsZeroStatePromoFeature,
              {{feature_engagement::kIPHExtensionsZeroStatePromoVariantParam
                    .name,
                feature_engagement::kIPHExtensionsZeroStatePromoVariantParam
                    .GetName(iphVariant)}}}})) {}

  void PreRunTestOnMainThread() override {
    // Block zero state promo IPH during browser launch, to prevent a race
    // condition where the IPH steals focus from the browser, and causes the
    // PreRunTestOnMainThread method to time out waiting for the browser to
    // come to focus.
    auto auto_reset = ExtensionsToolbarContainerViewController::
        BlockZeroStatePromoForTesting();
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  auto WaitForTabCount(int expected_count) {
    return Steps(
        PollState(kTabCountState,
                  [this]() { return browser()->tab_strip_model()->count(); }),
        WaitForState(kTabCountState, expected_count),
        StopObservingState(kTabCountState));
  }
};

class ExtensionsZeroStateCustomActionIphTest
    : public ExtensionsZeroStatePromoTestBase {
 public:
  ExtensionsZeroStateCustomActionIphTest()
      : ExtensionsZeroStatePromoTestBase(
            feature_engagement::IPHExtensionsZeroStatePromoVariant::
                kCustomActionIph) {}
};

// Test showing the zero state promo custom action IPH to users with no
// extensions installed. Clicking on the custom action opens a page to the
// Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomActionIphTest,
                       ShowingZeroStatePromoCustomActionIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome ::kChromeUIAboutURL)),
      InAnyContext(WaitForPromo(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature)),
      PressDefaultPromoButton(), WaitForTabCount(2),
      InstrumentTab(kSecondTabContents, 1),
      WaitForWebContentsReady(kSecondTabContents,
                              extension_urls::GetWebstoreLaunchURL()));
}
