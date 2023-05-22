// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/test_utils.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
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

// Build a ProductInfo with the specified cluster ID and image URL. If the image
// URL is not specified, it is left empty in the info object.
absl::optional<ProductInfo> CreateProductInfo(uint64_t cluster_id,
                                              const GURL& url = GURL()) {
  absl::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  if (!url.is_empty()) {
    info->image_url = url;
  }
  return info;
}

}  // namespace

class ShoppingListUiTabHelperTest : public testing::Test {
 public:
  ShoppingListUiTabHelperTest()
      : shopping_service_(std::make_unique<MockShoppingService>()),
        bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        image_fetcher_(std::make_unique<image_fetcher::MockImageFetcher>()) {}

  ShoppingListUiTabHelperTest(const ShoppingListUiTabHelperTest&) = delete;
  ShoppingListUiTabHelperTest operator=(const ShoppingListUiTabHelperTest&) =
      delete;
  ~ShoppingListUiTabHelperTest() override = default;

  void SetUp() override {
    web_contents_ = test_web_contents_factory_.CreateWebContents(&profile_);
    ShoppingListUiTabHelper::CreateForWebContents(
        web_contents_.get(), shopping_service_.get(), bookmark_model_.get(),
        image_fetcher_.get());
    tab_helper_ = ShoppingListUiTabHelper::FromWebContents(web_contents_.get());
  }

  void TestBody() override {}

  void TearDown() override {
    // Make sure the tab helper id destroyed before any of its dependencies are.
    tab_helper_ = nullptr;
    web_contents_->RemoveUserData(ShoppingListUiTabHelper::UserDataKey());
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
                                 const absl::optional<ProductInfo>& info) {
    tab_helper_->HandleProductInfoResponse(url, info);
  }

  // Passthrough methods for access to protected members.
  const absl::optional<bool>& GetPendingTrackingStateForTesting() {
    return tab_helper_->GetPendingTrackingStateForTesting();
  }

  void EnableChipExperimentVariation(
      base::test::ScopedFeatureList& feature_list,
      commerce::PriceTrackingChipExperimentVariation variation) {
    int variation_num = static_cast<int>(variation);
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams chip_experiment_param;
    chip_experiment_param
        [commerce::kCommercePriceTrackingChipExperimentVariationParam] =
            base::NumberToString(variation_num);
    enabled_features.emplace_back(
        commerce::kCommercePriceTrackingChipExperiment, chip_experiment_param);
    feature_list.InitWithFeaturesAndParameters(enabled_features,
                                               /*disabled_features*/ {});
  }

 protected:
  raw_ptr<ShoppingListUiTabHelper> tab_helper_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  // Must outlive `web_contents_`.
  content::TestWebContentsFactory test_web_contents_factory_;

 protected:
  raw_ptr<content::WebContents> web_contents_;
};

TEST_F(ShoppingListUiTabHelperTest, TestSubscriptionEventsUpdateState) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  absl::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));

  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  SimulateNavigationCommitted(GURL(kProductUrl));

  // First ensure that subscribe is successful.
  tab_helper_->OnSubscribe(CreateUserTrackedSubscription(kClusterId), true);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(tab_helper_->IsPriceTracking());

  // Now assume the user has unsubscribed again.
  shopping_service_->SetIsSubscribedCallbackValue(false);
  tab_helper_->OnUnsubscribe(CreateUserTrackedSubscription(kClusterId), true);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(tab_helper_->IsPriceTracking());
}

// The price tracking icon shouldn't be available if no image URL was provided
// by the shopping service.
TEST_F(ShoppingListUiTabHelperTest, TestIconAvailabilityIfNoImage) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  // Intentionally exclude the image here.
  absl::optional<ProductInfo> info = CreateProductInfo(kClusterId);
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
TEST_F(ShoppingListUiTabHelperTest, TestIconAvailabilityWithImage) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  absl::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));
  SetupImageFetcherForSimpleImage();

  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(tab_helper_->ShouldShowPriceTrackingIconView());
  ASSERT_EQ(GURL(kProductImageUrl), tab_helper_->GetProductImageURL());
}

// A request to change the state of a subscription should be immediately
// reflected in the accessor "IsPriceTracking".
TEST_F(ShoppingListUiTabHelperTest,
       TestSubscriptionChangeImmediatelySetsState) {
  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  absl::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));

  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsSubscribedCallbackValue(false);
  shopping_service_->SetSubscribeCallbackValue(true);

  SimulateNavigationCommitted(GURL(kProductUrl));

  ASSERT_FALSE(GetPendingTrackingStateForTesting().has_value());
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  EXPECT_CALL(
      *shopping_service_,
      Subscribe(VectorHasSubscriptionWithId(base::NumberToString(kClusterId)),
                testing::_))
      .Times(1);

  tab_helper_->SetPriceTrackingState(true, true, base::DoNothing());
  ASSERT_TRUE(GetPendingTrackingStateForTesting().has_value());
  ASSERT_TRUE(GetPendingTrackingStateForTesting().value());
  ASSERT_TRUE(tab_helper_->IsPriceTracking());
  base::RunLoop().RunUntilIdle();

  // We should still be price tracking, but there should no longer be a pending
  // value.
  ASSERT_FALSE(GetPendingTrackingStateForTesting().has_value());
  ASSERT_TRUE(tab_helper_->IsPriceTracking());
}

// Make sure unsubscribe without a bookmark for the current page is functional.
TEST_F(ShoppingListUiTabHelperTest, TestSubscriptionChangeNoBookmark) {
  // Intentionally create a bookmark with a URL different from the known
  // product URL but use the same cluster ID.
  AddProductBookmark(bookmark_model_.get(), u"title",
                     GURL("https://example.com/different_url.html"), kClusterId,
                     true);

  absl::optional<ProductInfo> info =
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

  // We should still be price tracking, but there should no longer be a pending
  // value.
  ASSERT_FALSE(tab_helper_->IsPriceTracking());
}

// The following tests are for the chip experiment - chip delay variation.
TEST_F(ShoppingListUiTabHelperTest, TestIconAvailableAfterLoading) {
  base::test::ScopedFeatureList feature_list;
  EnableChipExperimentVariation(
      feature_list, commerce::PriceTrackingChipExperimentVariation::kDelayChip);
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  absl::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));
  SetupImageFetcherForSimpleImage();

  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  // Simulate the navigation is committed and has stopped loading.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kProductUrl), web_contents_);
  simulator->SetKeepLoading(false);
  simulator->Start();
  simulator->Commit();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_helper_->GetProductImage().IsEmpty());
  EXPECT_TRUE(tab_helper_->ShouldShowPriceTrackingIconView());
}

TEST_F(ShoppingListUiTabHelperTest, TestIconNotAvailableDuringLoading) {
  base::test::ScopedFeatureList feature_list;
  EnableChipExperimentVariation(
      feature_list, commerce::PriceTrackingChipExperimentVariation::kDelayChip);
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  absl::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));
  SetupImageFetcherForSimpleImage();

  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  // Simulate the navigation is committed but has not stopped loading.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kProductUrl), web_contents_);
  simulator->SetKeepLoading(true);
  simulator->Start();
  simulator->Commit();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_helper_->GetProductImage().IsEmpty());
  EXPECT_FALSE(tab_helper_->ShouldShowPriceTrackingIconView());
}

}  // namespace commerce
