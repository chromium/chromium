// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_page_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/browser_with_test_window_test.h"

namespace extensions {

class ZeroStatePromoPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  ZeroStatePromoPageHandlerTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_ = std::make_unique<ZeroStatePromoPageHandler>(
        mojo::PendingReceiver<zero_state_promo::mojom::PageHandler>());
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Start the test with 1 active tab.
    AddTab(browser(), GURL("about:blank"));
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

  void ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked link,
      base::HistogramBase::Count32 expected_count) {
    histogram_tester_->ExpectBucketCount(
        "Extension.ZeroStatePromo.IphActionChromeWebStoreLink", link,
        expected_count);
  }

  ZeroStatePromoPageHandler& handler() { return *handler_; }

 protected:
  std::unique_ptr<ZeroStatePromoPageHandler> handler_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(ZeroStatePromoPageHandlerTest, LaunchDiscoverWebStoreLink) {
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension, 0);

  handler().LaunchWebStoreLink(
      zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(zero_state_promo::mojom::kDiscoverExtensionWebStoreUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension, 1);
}

TEST_F(ZeroStatePromoPageHandlerTest, LaunchCouponLink) {
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 0);

  handler().LaunchWebStoreLink(
      zero_state_promo::mojom::WebStoreLinkClicked::kCoupon);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(zero_state_promo::mojom::kCouponWebStoreUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kCoupon, 1);
}

TEST_F(ZeroStatePromoPageHandlerTest, LaunchWritingLink) {
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 0);

  handler().LaunchWebStoreLink(
      zero_state_promo::mojom::WebStoreLinkClicked::kWriting);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(zero_state_promo::mojom::kWritingWebStoreUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kWriting, 1);
}

TEST_F(ZeroStatePromoPageHandlerTest, LaunchProductivityLink) {
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 0);

  handler().LaunchWebStoreLink(
      zero_state_promo::mojom::WebStoreLinkClicked::kProductivity);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(zero_state_promo::mojom::kProductivityWebStoreUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kProductivity, 1);
}

TEST_F(ZeroStatePromoPageHandlerTest, LaunchAiLink) {
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kAi, 0);

  handler().LaunchWebStoreLink(
      zero_state_promo::mojom::WebStoreLinkClicked::kAi);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(zero_state_promo::mojom::kAiWebStoreUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  ExpectZeroStatePromoLinkClickCount(
      zero_state_promo::mojom::WebStoreLinkClicked::kAi, 1);
}
}  // namespace extensions
