// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/test_utils.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace commerce {

namespace {

const char kProductUrl[] = "http://example.com";
const char kProductImageUrl[] = "http://example.com/image.png";
const uint64_t kClusterId = 12345L;
const char kProductClusterTitle[] = "Product Cluster Title";

// Build a ProductInfo with the specified cluster ID, image URL and cluster
// title.
//   * If the image URL is not specified, it is left empty in the info object.
//   * If the cluster_title is not specified, it is left empty in the info
//   object.
std::optional<ProductInfo> CreateProductInfo(
    uint64_t cluster_id,
    const GURL& url = GURL(),
    const std::string cluster_title = std::string()) {
  std::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  if (!url.is_empty()) {
    info->image_url = url;
  }
  if (!cluster_title.empty()) {
    info->product_cluster_title = cluster_title;
  }
  return info;
}

}  // namespace

using ukm::builders::Shopping_ShoppingInformation;

class CommerceUiTabHelperTest : public testing::Test {
 public:
  CommerceUiTabHelperTest()
      : shopping_service_(std::make_unique<MockShoppingService>()),
        image_fetcher_(std::make_unique<image_fetcher::MockImageFetcher>()) {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
  }

  CommerceUiTabHelperTest(const CommerceUiTabHelperTest&) = delete;
  CommerceUiTabHelperTest operator=(const CommerceUiTabHelperTest&) =
      delete;
  ~CommerceUiTabHelperTest() override = default;

  void SetUp() override {
    web_contents_ = test_web_contents_factory_.CreateWebContents(&profile_);
    side_panel_registry_ = std::make_unique<SidePanelRegistry>(
        static_cast<tabs::TabInterface*>(nullptr));
    tab_helper_ = std::make_unique<commerce::CommerceUiTabHelper>(
        web_contents_.get(), shopping_service_.get(), bookmark_model_.get(),
        image_fetcher_.get(), side_panel_registry_.get());
  }

  void TestBody() override {}

  void TearDown() override {
    // Make sure the tab helper id destroyed before any of its dependencies are.
    tab_helper_.reset();
  }

  void SetupImageFetcherForSimpleImage() {
    ON_CALL(*image_fetcher_, FetchImageAndData_)
        .WillByDefault(
            [](const GURL& image_url,
               image_fetcher::ImageDataFetcherCallback* image_data_callback,
               image_fetcher::ImageFetcherCallback* image_callback,
               image_fetcher::ImageFetcherParams params) {
              SkBitmap bitmap;
              bitmap.allocN32Pixels(1, 1);
              gfx::Image image =
                  gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

              std::move(*image_callback)
                  .Run(std::move(image), image_fetcher::RequestMetadata());
            });
  }

  void SimulateNavigationCommitted(const GURL& url) {
    auto* web_content_tester =
        content::WebContentsTester::For(web_contents_.get());
    web_content_tester->SetLastCommittedURL(url);
    web_content_tester->NavigateAndCommit(url);
    web_content_tester->TestDidFinishLoad(url);

    base::RunLoop().RunUntilIdle();
  }

  // Passthrough to private methods in ShoppindListUiTabHelper:
  void HandleProductInfoResponse(const GURL& url,
                                 const std::optional<ProductInfo>& info) {
    tab_helper_->HandleProductInfoResponse(url, info);
  }

  // Passthrough methods for access to protected members.
  const std::optional<bool>& GetPendingTrackingStateForTesting() {
    return tab_helper_->GetPendingTrackingStateForTesting();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<CommerceUiTabHelper> tab_helper_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;

 private:
  TestingProfile profile_;

  // Must outlive `web_contents_`.
  content::TestWebContentsFactory test_web_contents_factory_;

 protected:
  raw_ptr<content::WebContents> web_contents_;
};

// The price tracking icon shouldn't be available if no image URL was provided
// by the shopping service.
TEST_F(CommerceUiTabHelperTest,
       TestPriceTrackingIconAvailabilityIfNoImage) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  // Intentionally exclude the image here.
  std::optional<ProductInfo> info = CreateProductInfo(kClusterId);
  // We do the setup for the image fetcher, but it won't be used since the
  // shopping service doesn't return an image URL.
  SetupImageFetcherForSimpleImage();

  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(tab_helper_->ShouldShowPriceTrackingIconView());
  ASSERT_TRUE(tab_helper_->GetProductImageURL().is_empty());
}

// The price tracking state should not update in the helper if there is no image
// returbed by the shopping service.
TEST_F(CommerceUiTabHelperTest,
       TestPriceTrackingIconAvailabilityWithImage) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  std::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));
  SetupImageFetcherForSimpleImage();

  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(tab_helper_->ShouldShowPriceTrackingIconView());
  ASSERT_EQ(GURL(kProductImageUrl), tab_helper_->GetProductImageURL());
}

// Make sure unsubscribe without a bookmark for the current page is functional.
TEST_F(CommerceUiTabHelperTest, TestSubscriptionChangeNoBookmark) {
  // Intentionally create a bookmark with a URL different from the known
  // product URL but use the same cluster ID.
  AddProductBookmark(bookmark_model_.get(), u"title",
                     GURL("https://example.com/different_url.html"), kClusterId,
                     true);

  std::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));

  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsSubscribedCallbackValue(true);
  shopping_service_->SetSubscribeCallbackValue(true);

  SimulateNavigationCommitted(GURL(kProductUrl));

  EXPECT_CALL(
      *shopping_service_,
      Unsubscribe(VectorHasSubscriptionWithId(base::NumberToString(kClusterId)),
                  testing::_))
      .Times(1);

  tab_helper_->SetPriceTrackingState(false, true, base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CommerceUiTabHelperTest, TestShoppingInsightsSidePanelAvailable) {
  ASSERT_FALSE(side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights)));

  shopping_service_->SetIsPriceInsightsEligible(true);

  std::optional<ProductInfo> product_info = CreateProductInfo(
      kClusterId, GURL(kProductImageUrl), kProductClusterTitle);
  shopping_service_->SetResponseForGetProductInfoForUrl(product_info);

  std::optional<PriceInsightsInfo> price_insights_info =
      CreateValidPriceInsightsInfo(true, true, PriceBucket::kLowPrice);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      price_insights_info);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights)));
}

TEST_F(CommerceUiTabHelperTest, TestShoppingInsightsSidePanelUnavailable) {
  ASSERT_FALSE(side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights)));

  shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);
  shopping_service_->SetIsPriceInsightsEligible(true);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights)));
}

TEST_F(CommerceUiTabHelperTest,
       TestPriceInsightsIconNotAvailableIfEmptyProductInfo) {
  shopping_service_->SetIsPriceInsightsEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);

  SimulateNavigationCommitted(GURL(kProductUrl));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tab_helper_->ShouldShowPriceInsightsIconView());
}

TEST_F(CommerceUiTabHelperTest,
       TestPriceInsightsIconNotAvailableIfNoProductClusterTitle) {
  shopping_service_->SetIsPriceInsightsEligible(true);

  std::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  SimulateNavigationCommitted(GURL(kProductUrl));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tab_helper_->ShouldShowPriceInsightsIconView());
}

TEST_F(CommerceUiTabHelperTest, TestRecordShoppingInformationUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  shopping_service_->SetIsPriceInsightsEligible(true);

  std::optional<ProductInfo> product_info = CreateProductInfo(
      kClusterId, GURL(kProductImageUrl), kProductClusterTitle);
  shopping_service_->SetResponseForGetProductInfoForUrl(product_info);

  std::optional<PriceInsightsInfo> price_insights_info =
      CreateValidPriceInsightsInfo(true, true, PriceBucket::kLowPrice);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      price_insights_info);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  auto entries =
      ukm_recorder.GetEntriesByName(Shopping_ShoppingInformation::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], Shopping_ShoppingInformation::kHasPriceInsightsName, 1);
  ukm_recorder.ExpectEntryMetric(
      entries[0], Shopping_ShoppingInformation::kIsPriceTrackableName, 1);
  ukm_recorder.ExpectEntryMetric(
      entries[0], Shopping_ShoppingInformation::kIsShoppingContentName, 1);
  ukm_recorder.ExpectEntryMetric(
      entries[0], Shopping_ShoppingInformation::kHasDiscountName, 0);
}

}  // namespace commerce
