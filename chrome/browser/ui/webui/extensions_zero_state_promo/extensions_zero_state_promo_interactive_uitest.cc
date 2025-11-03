// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"
#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble.h"
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kZeroStatePromoWebUiIphId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<GURL>,
                                    kOpenedTabUrlState);
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

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    ExtensionsToolbarContainerViewController::WakeZeroStatePromoForTesting();
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
          "Extensions.ZeroStatePromo.IphActionChromeWebStoreLink", link,
          expected_count);
    });
  }

  // Waits for the nth `tab` to be open to `url`. Doesn't require `url` to
  // load, as a test machine may not actually have connectivity to the internet.
  auto WaitForTabOpenedTo(int tab, GURL url) {
    return Steps(
        PollState(
            kOpenedTabUrlState,
            [this, tab]() {
              auto* const model = browser()->tab_strip_model();
              if (model->active_index() != tab) {
                return GURL();
              }
              return model->GetTabAtIndex(tab)->GetContents()->GetVisibleURL();
            }),
        WaitForState(kOpenedTabUrlState, url),
        StopObservingState(kOpenedTabUrlState));
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
                       ShowingZeroStatePromoCustomActionIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      InAnyContext(WaitForPromo(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature)),
      PressDefaultPromoButton(),
      WaitForTabOpenedTo(1,
                         GURL("https://chromewebstore.google.com"
                              "?utm_source=ext_zero_state_promo_generic_iph")));
}

// Test that IPH does not show when the user does not have the PromotionEnabled
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomActionIphTest,
                       RespectPromotionEnabledPolicy) {
  g_browser_process->local_state()->SetBoolean(prefs::kPromotionsEnabled,
                                               false);
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      InAnyContext(CheckPromoRequested(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature, false)));
}

class ExtensionsZeroStateCustomUiChipIphTestV1
    : public ExtensionsZeroStatePromoTestBase {
 public:
  ExtensionsZeroStateCustomUiChipIphTestV1()
      : ExtensionsZeroStatePromoTestBase(
            feature_engagement::IPHExtensionsZeroStatePromoVariant::
                kCustomUiChipIphV1) {}

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
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV1,
                       ClickCouponChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1,
          GURL("https://chromewebstore.google.com/category/extensions/"
               "lifestyle/shopping?utm_source=ext_zero_state_promo_chips_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the writing chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV1,
                       ClickWritingChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1,
          GURL("https://chromewebstore.google.com/collection/"
               "writing_essentials?utm_source=ext_zero_state_promo_chips_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the productivity chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV1,
                       ClickProductivityChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/collection/"
                  "productivity?utm_source=ext_zero_state_promo_chips_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the ai chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV1,
                       ClickAiChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/collection/"
                  "ai_productivity?utm_source=ext_zero_state_promo_chips_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kAi, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV1,
                       DismissPromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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

// Test that IPH does not show when the user does not have the PromotionEnabled
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV1,
                       RespectPromotionEnabledPolicy) {
  g_browser_process->local_state()->SetBoolean(prefs::kPromotionsEnabled,
                                               false);
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      InAnyContext(CheckPromoRequested(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature, false)));
}

class ExtensionsZeroStateCustomUiChipIphTestV2
    : public ExtensionsZeroStatePromoTestBase {
 public:
  ExtensionsZeroStateCustomUiChipIphTestV2()
      : ExtensionsZeroStatePromoTestBase(
            feature_engagement::IPHExtensionsZeroStatePromoVariant::
                kCustomUiChipIphV2) {}

  const DeepQuery kDismissButton{"extensions-zero-state-promo-app",
                                 "#dismissButton"};
  const DeepQuery kCouponButton{"extensions-zero-state-promo-app",
                                "#couponsButton"};
  const DeepQuery kWebstoreButton{"extensions-zero-state-promo-app",
                                  "#webStoreButton"};
  const DeepQuery kProductivityButton{"extensions-zero-state-promo-app",
                                      "#productivityButton"};
  const DeepQuery kAiButton{"extensions-zero-state-promo-app", "#aiButton"};
};

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the coupon chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV2,
                       ClickCouponChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/category/extensions/"
                  "lifestyle/"
                  "shopping?utm_source=ext_zero_state_promo_chips_iph_v2")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the web store chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV2,
                       ClickWebstoreChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kWebstoreButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabOpenedTo(1, GURL("https://"
                                 "chromewebstore.google.com/?utm_source="
                                 "ext_zero_state_promo_chips_iph_v2")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the productivity chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV2,
                       ClickProductivityChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/collection/"
                  "productivity?utm_source=ext_zero_state_promo_chips_iph_v2")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the ai chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV2,
                       ClickAiChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1,
          GURL("https://chromewebstore.google.com/collection/"
               "ai_productivity?utm_source=ext_zero_state_promo_chips_iph_v2")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kAi, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV2,
                       DismissPromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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

// Test that IPH does not show when the user does not have the PromotionEnabled
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV2,
                       RespectPromotionEnabledPolicy) {
  g_browser_process->local_state()->SetBoolean(prefs::kPromotionsEnabled,
                                               false);
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      InAnyContext(CheckPromoRequested(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature, false)));
}

class ExtensionsZeroStateCustomUiChipIphTestV3
    : public ExtensionsZeroStatePromoTestBase {
 public:
  ExtensionsZeroStateCustomUiChipIphTestV3()
      : ExtensionsZeroStatePromoTestBase(
            feature_engagement::IPHExtensionsZeroStatePromoVariant::
                kCustomUiChipIphV3) {}

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
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV3,
                       ClickCouponChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/category/extensions/"
                  "lifestyle/"
                  "shopping?utm_source=ext_zero_state_promo_chips_iph_v3")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the writing chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV3,
                       ClickWritingChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(1, GURL("https://chromewebstore.google.com/collection/"
                                 "writing_essentials?utm_source=ext_zero_state_"
                                 "promo_chips_iph_v3")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the productivity chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV3,
                       ClickProductivityChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/collection/"
                  "productivity?utm_source=ext_zero_state_promo_chips_iph_v3")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 1));
}

// Test showing the zero state promo custom ui IPH (chips variant) with no
// extensions installed. Clicking on the ai chip button should open a
// page to the Chrome Web Store.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV3,
                       ClickAiChipOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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
      WaitForTabOpenedTo(
          1,
          GURL("https://chromewebstore.google.com/collection/"
               "ai_productivity?utm_source=ext_zero_state_promo_chips_iph_v3")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kAi, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV3,
                       DismissPromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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

// Test that IPH does not show when the user does not have the PromotionEnabled
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiChipIphTestV3,
                       RespectPromotionEnabledPolicy) {
  g_browser_process->local_state()->SetBoolean(prefs::kPromotionsEnabled,
                                               false);
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      InAnyContext(CheckPromoRequested(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature, false)));
}

class ExtensionsZeroStateCustomUiPlainLinkIphTest
    : public ExtensionsZeroStatePromoTestBase {
 public:
  ExtensionsZeroStateCustomUiPlainLinkIphTest()
      : ExtensionsZeroStatePromoTestBase(
            feature_engagement::IPHExtensionsZeroStatePromoVariant::
                kCustomUIPlainLinkIph) {}

  const DeepQuery kDismissButton{"extensions-zero-state-promo-app",
                                 "#dismissButton"};
  const DeepQuery kCouponLink{"extensions-zero-state-promo-app",
                              "#couponsLink"};
  const DeepQuery kWritingLink{"extensions-zero-state-promo-app",
                               "#writingLink"};
  const DeepQuery kProductivityLink{"extensions-zero-state-promo-app",
                                    "#productivityLink"};
  const DeepQuery kAiLink{"extensions-zero-state-promo-app", "#aiLink"};
  const DeepQuery kCloseButton{"extensions-zero-state-promo-app",
                               "#closeButton"};
  const DeepQuery kDiscoverExtensionsButton{"extensions-zero-state-promo-app",
                                            "#customActionButton"};
  const DeepQuery kGotItButton{"extensions-zero-state-promo-app",
                               "#closeButton"};
};

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       ClickCouponLinkOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kCouponLink,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabOpenedTo(
          1,
          GURL("https://chromewebstore.google.com/category/extensions/"
               "lifestyle/shopping?utm_source=ext_zero_state_promo_links_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 1));
}

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       ClickWritingLinkOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kWritingLink,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabOpenedTo(
          1,
          GURL("https://chromewebstore.google.com/collection/"
               "writing_essentials?utm_source=ext_zero_state_promo_links_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 1));
}

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       ClickProductivityLinkOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kProductivityLink,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/collection/"
                  "productivity?utm_source=ext_zero_state_promo_links_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 1));
}

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       ClickAiLinkOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kAi, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kAiLink,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabOpenedTo(
          1, GURL("https://chromewebstore.google.com/collection/"
                  "ai_productivity?utm_source=ext_zero_state_promo_links_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kAi, 1));
}

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       ClickDiscoverExtensionsButtonOnZeroStatePromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension, 0),
      ClickElement(kZeroStatePromoWebUiIphId, kDiscoverExtensionsButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForTabOpenedTo(1, GURL("https://"
                                 "chromewebstore.google.com?utm_source=ext_"
                                 "zero_state_promo_links_iph")),
      CheckZeroStatePromoLinkClickCount(
          zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension, 1));
}

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       DismissPromoIph) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
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

// TODO(crbug.com/419854475): Re-enable this test once the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       ClickGotItButton) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kZeroStatePromoWebUiIphId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(
          kZeroStatePromoWebUiIphId,
          GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
      ClickElement(kZeroStatePromoWebUiIphId, kGotItButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kWebViewIdForTesting),
      CheckResult(
          [this] { return browser()->tab_strip_model()->GetTabCount(); }, 1,
          "CheckTabCount"),
      CheckZeroStatePromoClosedReason(
          user_education::FeaturePromoClosedReason::kDismiss));
}

// Test that IPH does not show when the user does not have the PromotionEnabled
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionsZeroStateCustomUiPlainLinkIphTest,
                       RespectPromotionEnabledPolicy) {
  g_browser_process->local_state()->SetBoolean(prefs::kPromotionsEnabled,
                                               false);
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUIAboutURL)),
      InAnyContext(CheckPromoRequested(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature, false)));
}
