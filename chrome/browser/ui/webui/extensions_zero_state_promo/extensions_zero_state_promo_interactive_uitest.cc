// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"
#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble.h"
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kZeroStatePromoWebUiIphId);
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

  auto CheckZeroStatePromoClosedReason(
      user_education::FeaturePromoClosedReason closeReason) {
    return Do([this, closeReason]() {
      histogram_tester_.ExpectBucketCount(
          "UserEducation.MessageAction.IPH_ExtensionsZeroStatePromo",
          closeReason, 1);
    });
  }

  auto CheckZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked link,
      base::HistogramBase::Count32 expected_count) {
    return Do([this, link, expected_count]() {
      histogram_tester_.ExpectBucketCount(
          "Extension.ZeroStatePromo.IphActionChromeWebStoreLink", link,
          expected_count);
    });
  }

 private:
  base::HistogramTester histogram_tester_;
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
// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomActionIphTest,
                       DISABLED_ShowingZeroStatePromoCustomActionIph) {
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

class ExtensionsZeroStateCustomUiChipIphTest
    : public ExtensionsZeroStatePromoTestBase {
 public:
  ExtensionsZeroStateCustomUiChipIphTest()
      : ExtensionsZeroStatePromoTestBase(
            feature_engagement::IPHExtensionsZeroStatePromoVariant::
                kCustomUiChipIph) {}

  const DeepQuery kDismissButton{"extensions-zero-state-promo-app",
                                 "#dismissButton"};
  const DeepQuery kCouponButton{"extensions-zero-state-promo-app",
                                "#couponsButton"};
  const DeepQuery kWritingButton{"extensions-zero-state-promo-app",
                                 "#writingButton"};
  const DeepQuery kProductivityButton{"extensions-zero-state-promo-app",
                                      "#productivityButton"};
  const DeepQuery kAiButton{"extensions-zero-state-promo-app", "#aiButton"};
};

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the coupon chip button should open a
// page to the Chrome Web Store.
// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTest,
                       DISABLED_ClickCouponChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome ::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kCouponButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabCount(2), InstrumentTab(kSecondTabContents, 1),
      WaitForWebContentsReady(kSecondTabContents,
                              GURL("https://chromewebstore.google.com/category/"
                                   "extensions/lifestyle/shopping")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the writing chip button should open a
// page to the Chrome Web Store.
// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTest,
                       DISABLED_ClickWritingChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome ::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kWritingButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabCount(2), InstrumentTab(kSecondTabContents, 1),
      WaitForWebContentsReady(kSecondTabContents,
                              GURL("https://chromewebstore.google.com/"
                                   "collection/writing_essentials")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the productivity chip button should open a
// page to the Chrome Web Store.
// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTest,
                       DISABLED_ClickProductivityChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome ::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kProductivityButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabCount(2), InstrumentTab(kSecondTabContents, 1),
      WaitForWebContentsReady(kSecondTabContents,
                              GURL("https://chromewebstore.google.com/category/"
                                   "extensions/productivity/workflow")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the ai chip button should open a
// page to the Chrome Web Store.
// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTest,
                       DISABLED_ClickAiChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome ::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kAi, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kAiButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabCount(2), InstrumentTab(kSecondTabContents, 1),
      WaitForWebContentsReady(
          kSecondTabContents,
          GURL("https://chromewebstore.google.com/collection/ai_productivity")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kAi, 1));
}

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTest,
                       DISABLED_DismissPromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome ::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      ClickElement(kZeroStatePromoWebUiIphId, kDismissButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      CheckResult(
          [this] { return browser()->tab_strip_model()->GetTabCount(); }, 1,
          "CheckTabCount"),
      CheckZeroStatePromoClosedReason(
          user_education::FeaturePromoClosedReason::kDismiss));
}
