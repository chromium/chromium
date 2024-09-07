// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/search/ntp_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShoppingTab);

const char kShoppingURL[] = "/shopping.html";
const char kNonShoppingURL[] = "/non-shopping.html";
const char kProductClusterTitle[] = "Product Cluster Title";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("page content");
  response->set_content_type("text/html");
  return response;
}
}  // namespace

class PriceInsightsIconViewInteractiveTest
    : public InteractiveFeaturePromoTest {
 public:
  explicit PriceInsightsIconViewInteractiveTest(
      std::vector<base::test::FeatureRef> iph_features = {})
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos(std::move(iph_features))) {
    test_features_.InitWithFeatures(
        /*enabled_features=*/{commerce::kCommerceAllowChipExpansion,
                              commerce::kPriceInsights},
        /*disabled_features*/ {});
  }

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&BasicResponse));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveFeaturePromoTest::SetUpOnMainThread();

    SetUpTabHelperAndShoppingService();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&PriceInsightsIconViewInteractiveTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    is_browser_context_services_created = false;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    is_browser_context_services_created = true;
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

 protected:
  raw_ptr<commerce::MockShoppingService, AcrossTasksDanglingUntriaged>
      mock_shopping_service_;
  std::optional<commerce::PriceInsightsInfo> price_insights_info_;
  std::optional<commerce::ProductInfo> product_info_;
  base::CallbackListSubscription create_services_subscription_;
  bool is_browser_context_services_created{false};

 private:
  base::test::ScopedFeatureList test_features_;

  void SetUpTabHelperAndShoppingService() {
    EXPECT_TRUE(is_browser_context_services_created);
    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));

    price_insights_info_ = commerce::CreateValidPriceInsightsInfo(
        true, true, commerce::PriceBucket::kLowPrice);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info_);

    product_info_ = commerce::ProductInfo();
    product_info_->title = "Product";
    product_info_->product_cluster_title = "Product";
    product_info_->product_cluster_id = 12345L;
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(product_info_);

    EXPECT_CALL(*mock_shopping_service_, IsPriceInsightsEligible)
        .Times(testing::AnyNumber());

    mock_shopping_service_->SetIsPriceInsightsEligible(true);
    mock_shopping_service_->SetIsShoppingListEligible(false);
    mock_shopping_service_->SetIsDiscountEligibleToShowOnNavigation(false);

    MockGetProductInfoForUrlResponse();
    MockGetPriceInsightsInfoForUrlResponse();
  }

  void MockGetProductInfoForUrlResponse() {
    commerce::ProductInfo info;
    info.product_cluster_title = kProductClusterTitle;
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(info);
  }

  void MockGetPriceInsightsInfoForUrlResponse() {
    std::optional<commerce::PriceInsightsInfo> price_insights_info =
        commerce::CreateValidPriceInsightsInfo(
            true, true, commerce::PriceBucket::kLowPrice);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info);
  }

  base::WeakPtrFactory<PriceInsightsIconViewInteractiveTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewInteractiveTest,
                       SidePanelShownOnPress) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl)
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl)
      .Times(testing::AnyNumber());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),

      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click on the action chip to open the side panel
      PressButton(kPriceInsightsChipElementId),
      WaitForShow(kSidePanelElementId),
      // Click on the action chip again to close the side panel
      PressButton(kPriceInsightsChipElementId),
      WaitForHide(kSidePanelElementId));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::Shopping_ShoppingAction::kPriceInsightsOpenedName, 1);
  ukm_recorder.ExpectEntrySourceHasUrl(
      entries[0], embedded_test_server()->GetURL(kShoppingURL));
}

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewInteractiveTest,
                       IconIsNotHighlightedAfterClicking) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl);
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl);

  const bool expected_to_highlight = false;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceInsightsChipElementId),
      PressButton(kPriceInsightsChipElementId),
      CheckView(
          kPriceInsightsChipElementId,
          [](PriceInsightsIconView* icon) {
            return icon->IsIconHighlightedForTesting();
          },
          expected_to_highlight));
}

class PriceInsightsIconViewEngagementTest
    : public PriceInsightsIconViewInteractiveTest {
 public:
  PriceInsightsIconViewEngagementTest()
      : PriceInsightsIconViewInteractiveTest(
            {feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature}) {
  }

  void SetUpOnMainThread() override {
    PriceInsightsIconViewInteractiveTest::SetUpOnMainThread();
    RunTestSequence(InstrumentTab(kShoppingTab));
  }

  void NavigateToANonShoppingPage() {
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        std::nullopt);
    RunTestSequence(
        NavigateWebContents(kShoppingTab,
                            embedded_test_server()->GetURL(kNonShoppingURL)),
        WaitForHide(kPriceInsightsChipElementId));
  }

  void NavigateToAShoppingPage(bool expected_to_show_label) {
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(product_info_);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info_);
    RunTestSequence(
        NavigateWebContents(kShoppingTab,
                            embedded_test_server()->GetURL(kShoppingURL)),
        WaitForShow(kPriceInsightsChipElementId),
        CheckViewProperty(kPriceInsightsChipElementId,
                          &PriceInsightsIconView::ShouldShowLabel,
                          expected_to_show_label));
  }

  void VerifyIconExpanded() {
    base::HistogramTester histogram_tester;
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      0);

    NavigateToANonShoppingPage();
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      0);

    NavigateToAShoppingPage(/*expected_to_show_label=*/true);
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      1);
    histogram_tester.ExpectBucketCount(
        "Commerce.PriceInsights.OmniboxIconShown", 1, 1);

    NavigateToANonShoppingPage();
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      1);
    histogram_tester.ExpectBucketCount(
        "Commerce.PriceInsights.OmniboxIconShown", 1, 1);

    NavigateToAShoppingPage(/*expected_to_show_label=*/true);
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      2);
    histogram_tester.ExpectBucketCount(
        "Commerce.PriceInsights.OmniboxIconShown", 1, 2);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Commerce.PriceInsights.OmniboxIconShown"),
        BucketsAre(base::Bucket(0, 0), base::Bucket(1, 2), base::Bucket(2, 0)));
  }
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewEngagementTest, ExpandedIconShown) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl)
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl)
      .Times(testing::AnyNumber());

  VerifyIconExpanded();
}
