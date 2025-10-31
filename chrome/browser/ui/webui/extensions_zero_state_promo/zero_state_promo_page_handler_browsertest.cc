// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_page_handler.h"

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_urls.h"
#include "url/gurl.h"

namespace extensions {

struct WebStoreLinkTestCase {
  zero_state_promo::mojom::WebStoreLinkClicked link_type;
  std::string_view url_constant;
};

class ZeroStatePromoPageHandlerTest : public InProcessBrowserTest {
 public:
  ZeroStatePromoPageHandlerTest() {
    feature_list_.InitAndEnableFeaturesWithParameters(
        {base::test::FeatureRefAndParams(
            feature_engagement::kIPHExtensionsZeroStatePromoFeature,
            {{feature_engagement::kIPHExtensionsZeroStatePromoVariantParam.name,
              feature_engagement::kIPHExtensionsZeroStatePromoVariantParam
                  .GetName(
                      feature_engagement::IPHExtensionsZeroStatePromoVariant::
                          kCustomUiChipIphV2)}})});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    handler_ = std::make_unique<ZeroStatePromoPageHandler>(
        GetProfile(),
        mojo::PendingReceiver<zero_state_promo::mojom::PageHandler>());
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Start the test with 1 active tab.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("about:blank"), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked link,
      base::HistogramBase::Count32 expected_count) {
    histogram_tester_->ExpectBucketCount(
        "Extensions.ZeroStatePromo.IphActionChromeWebStoreLink", link,
        expected_count);
  }

  ZeroStatePromoPageHandler& handler() { return *handler_; }

 protected:
  GURL GetExpectedUrlWithUtm(std::string_view base_url) {
    return extension_urls::AppendUtmSource(
        GURL(base_url), extension_urls::kCustomUiChipIphV2UtmSource);
  }

  void TestWebStoreLink(const WebStoreLinkTestCase& test_case) {
    ExpectZeroStatePromoLinkClickCount(test_case.link_type, 0);

    handler().LaunchWebStoreLink(test_case.link_type);

    ASSERT_EQ(2, browser()->tab_strip_model()->count());
    GURL expected_url = GetExpectedUrlWithUtm(test_case.url_constant);
    ASSERT_EQ(expected_url,
              browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
    ExpectZeroStatePromoLinkClickCount(test_case.link_type, 1);
  }

  std::unique_ptr<ZeroStatePromoPageHandler> handler_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZeroStatePromoPageHandlerTest,
                       LaunchDiscoverWebStoreLink) {
  TestWebStoreLink(
      {.link_type =
           zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension,
       .url_constant = zero_state_promo::mojom::kDiscoverExtensionWebStoreUrl});
}

IN_PROC_BROWSER_TEST_F(ZeroStatePromoPageHandlerTest, LaunchCouponLink) {
  TestWebStoreLink(
      {.link_type = zero_state_promo::mojom::WebStoreLinkClicked::kCoupon,
       .url_constant = zero_state_promo::mojom::kCouponWebStoreUrl});
}

IN_PROC_BROWSER_TEST_F(ZeroStatePromoPageHandlerTest, LaunchWritingLink) {
  TestWebStoreLink(
      {.link_type = zero_state_promo::mojom::WebStoreLinkClicked::kWriting,
       .url_constant = zero_state_promo::mojom::kWritingWebStoreUrl});
}

IN_PROC_BROWSER_TEST_F(ZeroStatePromoPageHandlerTest, LaunchProductivityLink) {
  TestWebStoreLink(
      {.link_type = zero_state_promo::mojom::WebStoreLinkClicked::kProductivity,
       .url_constant = zero_state_promo::mojom::kProductivityWebStoreUrl});
}

IN_PROC_BROWSER_TEST_F(ZeroStatePromoPageHandlerTest, LaunchAiLink) {
  TestWebStoreLink(
      {.link_type = zero_state_promo::mojom::WebStoreLinkClicked::kAi,
       .url_constant = zero_state_promo::mojom::kAiWebStoreUrl});
}
}  // namespace extensions
