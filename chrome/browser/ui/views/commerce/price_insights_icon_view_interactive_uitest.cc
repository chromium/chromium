// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search/ntp_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/animation/ink_drop.h"
#include "url/gurl.h"

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

class PriceInsightsIconViewBaseInteractiveTest
    : public PageActionInteractiveTestMixin<InteractiveFeaturePromoTest> {
 public:
  explicit PriceInsightsIconViewBaseInteractiveTest(
      bool is_migration_enabled,
      std::vector<base::test::FeatureRef> iph_features = {})
      : PageActionInteractiveTestMixin(
            UseDefaultTrackerAllowingPromos(std::move(iph_features))) {
    if (is_migration_enabled) {
      test_features_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {
              {commerce::kPriceInsights, {{}}},
              {
                  ::features::kPageActionsMigration,
                  {
                      {::features::kPageActionsMigrationPriceInsights.name,
                       "true"},
                  },
              },

          },
          /*disabled_features*/ {commerce::kEnableDiscountInfoApi,
                                 commerce::kProductSpecifications});
    } else {
      test_features_.InitWithFeatures(
          /*enabled_features=*/{commerce::kPriceInsights},
          /*disabled_features*/ {commerce::kEnableDiscountInfoApi,
                                 commerce::kProductSpecifications,
                                 ::features::kPageActionsMigration});
    }
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
                base::BindRepeating(&PriceInsightsIconViewBaseInteractiveTest::
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
  std::unique_ptr<commerce::MockAccountChecker> mock_account_checker_;
  bool is_browser_context_services_created{false};

 private:
  base::test::ScopedFeatureList test_features_;

  void SetUpTabHelperAndShoppingService() {
    EXPECT_TRUE(is_browser_context_services_created);
    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
    mock_account_checker_ = std::make_unique<commerce::MockAccountChecker>();
    mock_shopping_service_->SetAccountChecker(mock_account_checker_.get());

    price_insights_info_ = commerce::CreateValidPriceInsightsInfo(
        true, true, commerce::PriceBucket::kLowPrice);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info_);

    product_info_ = commerce::ProductInfo();
    product_info_->title = "Product";
    product_info_->product_cluster_title = "Product";
    product_info_->product_cluster_id = 12345L;
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(product_info_);

    mock_account_checker_->SetAnonymizedUrlDataCollectionEnabled(true);
    ASSERT_TRUE(commerce::IsPriceInsightsEligible(mock_account_checker_.get()));
    mock_shopping_service_->SetIsShoppingListEligible(false);

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

  base::WeakPtrFactory<PriceInsightsIconViewBaseInteractiveTest>
      weak_ptr_factory_{this};
};

class PriceInsightsIconViewInteractiveTest
    : public PriceInsightsIconViewBaseInteractiveTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PriceInsightsIconViewInteractiveTest()
      : PriceInsightsIconViewBaseInteractiveTest(GetParam()) {}
};

IN_PROC_BROWSER_TEST_P(PriceInsightsIconViewInteractiveTest,
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

IN_PROC_BROWSER_TEST_P(PriceInsightsIconViewInteractiveTest,
                       IconIsNotHighlightedAfterClicking) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl)
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl)
      .Times(testing::AnyNumber());

  const bool expected_to_highlight = false;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceInsightsChipElementId),
      PressButton(kPriceInsightsChipElementId),
      CheckView(
          kPriceInsightsChipElementId,
          [](IconLabelBubbleView* icon) {
            return views::InkDrop::Get(icon)
                       ->GetInkDrop()
                       ->GetTargetInkDropState() ==
                   views::InkDropState::ACTIVATED;
          },
          expected_to_highlight));
}

// TODO(crbug.com/429709568): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_P(PriceInsightsIconViewInteractiveTest,
                       DISABLED_TabDiscardDuringNavigationNoCrash) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingStateObserver<mojom::LifecycleUnitState>,
      kShoppingTabState);

  constexpr char kEmptyDocumentURL[] = "/empty.html";

  // Get the LifecycleUnit for the first tab (kShoppingTab).
  auto* lifecycle_unit =
      resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
          browser()->tab_strip_model()->GetWebContentsAt(0));

  RunTestSequence(
      InstrumentTab(kShoppingTab),

      // Open a second tab with a blank page.
      AddInstrumentedTab(kSecondTab,
                         embedded_test_server()->GetURL(kEmptyDocumentURL)),

      SelectTab(kTabStripElementId, 0),

      // Navigate the shopping tab to a shopping-related URL and wait for the
      // chip to appear.
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceInsightsChipElementId),

      // Start a navigation to a non-shopping page but do not wait for it to
      // complete.
      Do(base::BindLambdaForTesting([&]() {
        ui_test_utils::NavigateToURLWithDisposition(
            browser(), embedded_test_server()->GetURL(kEmptyDocumentURL),
            WindowOpenDisposition::CURRENT_TAB,
            ui_test_utils::BROWSER_TEST_NO_WAIT);
      })),

      // Immediately switch to the second tab.
      SelectTab(kTabStripElementId, 1),

      // Discard the shopping tab while it is navigating.
      Do(base::BindLambdaForTesting([&]() {
        lifecycle_unit->DiscardTab(mojom::LifecycleUnitDiscardReason::EXTERNAL);
      })),

      // Ensure that the discard is completed.
      PollState(kShoppingTabState,
                [&]() { return lifecycle_unit->GetTabState(); }),
      WaitForState(kShoppingTabState, mojom::LifecycleUnitState::DISCARDED),
      StopObservingState(kShoppingTabState),

      // Switch back to the shopping tab. This causes it to reload.
      SelectTab(kTabStripElementId, 0),

      WaitForWebContentsReady(kShoppingTab),

      // After reload, the tab restores its last committed URL `shopping_url`,
      // so the Price Insights chip should be visible.
      WaitForShow(kPriceInsightsChipElementId));

  // There should not be a crash and the feature should continue working as
  // expected.
}

INSTANTIATE_TEST_SUITE_P(All,
                         PriceInsightsIconViewInteractiveTest,
                         ::testing::Values(false, true),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigrationEnabled"
                                             : "MigrationDisabled";
                         });

class PriceInsightsIconViewEngagementTest
    : public PriceInsightsIconViewBaseInteractiveTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PriceInsightsIconViewEngagementTest()
      : PriceInsightsIconViewBaseInteractiveTest(
            GetParam(),
            {feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature}) {
  }

  void SetUpOnMainThread() override {
    PriceInsightsIconViewBaseInteractiveTest::SetUpOnMainThread();
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

  using PageActionInteractiveTestMixin::WaitForPageActionChipVisible;

  auto WaitForPageActionChipVisible() {
    MultiStep steps;
    steps += WaitForPageActionChipVisible(kActionCommercePriceInsights);
    return steps;
  }

  void NavigateToAShoppingPage(bool expected_to_show_label) {
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(product_info_);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info_);
    RunTestSequence(
        NavigateWebContents(kShoppingTab,
                            embedded_test_server()->GetURL(kShoppingURL)),
        WaitForPageActionChipVisible(),
        CheckViewProperty(kPriceInsightsChipElementId,
                          &IconLabelBubbleView::ShouldShowLabel,
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

IN_PROC_BROWSER_TEST_P(PriceInsightsIconViewEngagementTest, ExpandedIconShown) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl)
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl)
      .Times(testing::AnyNumber());

  VerifyIconExpanded();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PriceInsightsIconViewEngagementTest,
                         ::testing::Values(false, true),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigrationEnabled"
                                             : "MigrationDisabled";
                         });
