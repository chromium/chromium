// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/price_tracking_page_action_controller.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace commerce {

namespace {

// Build a basic ProductInfo object.
std::optional<ProductInfo> CreateProductInfo(uint64_t cluster_id,
                                             const GURL& image_url = GURL()) {
  std::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  if (!image_url.is_empty()) {
    info->image_url = image_url;
  }
  return info;
}

}  // namespace

class PriceTrackingPageActionControllerUnittest : public testing::Test {
 public:
  PriceTrackingPageActionControllerUnittest()
      : shopping_service_(std::make_unique<MockShoppingService>()),
        image_fetcher_(std::make_unique<image_fetcher::MockImageFetcher>()),
        tracker_(std::make_unique<feature_engagement::test::MockTracker>()) {}

  PriceTrackingPageActionControllerUnittest(
      const PriceTrackingPageActionControllerUnittest&) = delete;
  PriceTrackingPageActionControllerUnittest operator=(
      const PriceTrackingPageActionControllerUnittest&) = delete;
  ~PriceTrackingPageActionControllerUnittest() override = default;

  void TestBody() override {}

  void SetupImageFetcherForSimpleImage(bool valid_image) {
    ON_CALL(*image_fetcher_, FetchImageAndData_)
        .WillByDefault(
            [valid_image = valid_image](
                const GURL& image_url,
                image_fetcher::ImageDataFetcherCallback* image_data_callback,
                image_fetcher::ImageFetcherCallback* image_callback,
                image_fetcher::ImageFetcherParams params) {
              if (valid_image) {
                SkBitmap bitmap;
                bitmap.allocN32Pixels(1, 1);
                gfx::Image image =
                    gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
                std::move(*image_callback)
                    .Run(std::move(image), image_fetcher::RequestMetadata());
              } else {
                std::move(*image_callback)
                    .Run(gfx::Image(), image_fetcher::RequestMetadata());
              }
            });
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::MockRepeatingCallback<void()> notify_host_callback_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher_;
  std::unique_ptr<feature_engagement::test::MockTracker> tracker_;
};

TEST_F(PriceTrackingPageActionControllerUnittest, IconShown) {
  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetIsSubscribedCallbackValue(false);
  SetupImageFetcherForSimpleImage(/*valid_image=*/true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller.WantsExpandedUi());

  uint64_t cluster_id = 12345L;
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              IsSubscribed(SubscriptionWithId(base::NumberToString(cluster_id)),
                           testing::_))
      .Times(1);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(cluster_id, GURL("http://example.com/image.png")));
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller.ShouldShowForNavigation().value());
}

TEST_F(PriceTrackingPageActionControllerUnittest, IconNotShown_NoImage) {
  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetIsSubscribedCallbackValue(false);
  SetupImageFetcherForSimpleImage(/*valid_image=*/false);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller.WantsExpandedUi());

  uint64_t cluster_id = 12345L;
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              IsSubscribed(SubscriptionWithId(base::NumberToString(cluster_id)),
                           testing::_))
      .Times(1);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(cluster_id, GURL("http://example.com/image.png")));
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller.ShouldShowForNavigation().value());
}

TEST_F(PriceTrackingPageActionControllerUnittest, IconNotShown_NoProductInfo) {
  shopping_service_->SetIsShoppingListEligible(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller.WantsExpandedUi());

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_, IsSubscribed(testing::_, testing::_))
      .Times(0);
  shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller.ShouldShowForNavigation().value());
}

TEST_F(PriceTrackingPageActionControllerUnittest, IconNotShown_NotEligible) {
  shopping_service_->SetIsShoppingListEligible(false);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller.ShouldShowForNavigation().value());
}

TEST_F(PriceTrackingPageActionControllerUnittest,
       SubscriptionEventsUpdateState) {
  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetIsSubscribedCallbackValue(false);
  SetupImageFetcherForSimpleImage(/*valid_image=*/true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller.WantsExpandedUi());

  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(12345L, GURL("http://example.com/image.png")));
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  CommerceSubscription sub(SubscriptionType::kPriceTrack,
                           IdentifierType::kProductClusterId, "12345",
                           ManagementType::kUserManaged);

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  shopping_service_->SetIsSubscribedCallbackValue(true);
  controller.OnSubscribe(sub, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  shopping_service_->SetIsSubscribedCallbackValue(false);
  controller.OnUnsubscribe(sub, true);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller.ShouldShowForNavigation().value());
}

TEST_F(PriceTrackingPageActionControllerUnittest, WantsExpandedUi_HighPrice) {
  shopping_service_->SetIsShoppingListEligible(true);
  SetupImageFetcherForSimpleImage(/*valid_image=*/true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  std::optional<ProductInfo> info =
      CreateProductInfo(12345L, GURL("http://example.com/image.png"));
  info->amount_micros = 150000000L;  // $150 in micros
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  // First run the case where the user isn't subscribed.
  shopping_service_->SetIsSubscribedCallbackValue(false);
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller.ShouldShowForNavigation().value());
  ASSERT_TRUE(controller.WantsExpandedUi());

  // If the user is already subscribed, we shouldn't expand regardless of the
  // price.
  shopping_service_->SetIsSubscribedCallbackValue(true);
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller.ShouldShowForNavigation().value());
  ASSERT_FALSE(controller.WantsExpandedUi());
}

TEST_F(PriceTrackingPageActionControllerUnittest, WantsExpandedUi_Tracker) {
  shopping_service_->SetIsShoppingListEligible(true);
  SetupImageFetcherForSimpleImage(/*valid_image=*/true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(12345L, GURL("http://example.com/image.png")));

  ON_CALL(*tracker_,
          ShouldTriggerHelpUI(testing::Ref(
              feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature)))
      .WillByDefault(testing::Return(true));

  // First run the case where the user isn't subscribed.
  shopping_service_->SetIsSubscribedCallbackValue(false);
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller.ShouldShowForNavigation().value());
  ASSERT_TRUE(controller.WantsExpandedUi());

  // If the user is already subscribed, we shouldn't access the tracker or
  // expand.
  shopping_service_->SetIsSubscribedCallbackValue(true);
  controller.ResetForNewNavigation(GURL("http://example.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller.ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller.ShouldShowForNavigation().value());
  ASSERT_FALSE(controller.WantsExpandedUi());
}

TEST_F(PriceTrackingPageActionControllerUnittest,
       IconInteractionStateMetrics_Expanded) {
  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetIsSubscribedCallbackValue(false);
  SetupImageFetcherForSimpleImage(/*valid_image=*/true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  std::optional<ProductInfo> info =
      CreateProductInfo(12345L, GURL("http://example.com/image.png"));
  info->amount_micros = 150000000L;  // $150 in micros
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  std::string histogram_name = "Commerce.PriceTracking.IconInteractionState";
  base::HistogramTester histogram_tester;

  // First, simulate a click on the expanded chip.
  controller.ResetForNewNavigation(GURL("http://example.com/1"));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller.WantsExpandedUi());
  controller.OnIconClicked();

  histogram_tester.ExpectTotalCount(histogram_name, 1);
  histogram_tester.ExpectBucketCount(
      histogram_name, PageActionIconInteractionState::kClickedExpanded, 1);

  // Simulate no click on an expanded chip.
  controller.ResetForNewNavigation(GURL("http://example.com/2"));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller.WantsExpandedUi());
  // Leave the page before a click happens.
  controller.ResetForNewNavigation(GURL("http://example.com/3"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(histogram_name, 2);
  histogram_tester.ExpectBucketCount(
      histogram_name, PageActionIconInteractionState::kNotClickedExpanded, 1);
}

TEST_F(PriceTrackingPageActionControllerUnittest,
       IconInteractionStateMetrics_NotExpanded) {
  shopping_service_->SetIsShoppingListEligible(true);

  // Simulate a product not being tracked.
  shopping_service_->SetIsSubscribedCallbackValue(false);

  SetupImageFetcherForSimpleImage(/*valid_image=*/true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(12345L, GURL("http://example.com/image.png")));

  std::string histogram_name = "Commerce.PriceTracking.IconInteractionState";
  base::HistogramTester histogram_tester;

  // Click on an unexpanded chip.
  controller.ResetForNewNavigation(GURL("http://example.com/1"));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(controller.WantsExpandedUi());
  controller.OnIconClicked();

  histogram_tester.ExpectTotalCount(histogram_name, 1);
  histogram_tester.ExpectBucketCount(
      histogram_name, PageActionIconInteractionState::kClicked, 1);

  // No click on an unexpanded chip.
  controller.ResetForNewNavigation(GURL("http://example.com/2"));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(controller.WantsExpandedUi());
  // Leave the page before a click happens.
  controller.ResetForNewNavigation(GURL("http://example.com/3"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(histogram_name, 2);
  histogram_tester.ExpectBucketCount(
      histogram_name, PageActionIconInteractionState::kNotClicked, 1);
}

TEST_F(PriceTrackingPageActionControllerUnittest,
       IconInteractionStateMetrics_AlreadyTracked) {
  shopping_service_->SetIsShoppingListEligible(true);

  // Simulate a product being tracked.
  shopping_service_->SetIsSubscribedCallbackValue(true);

  SetupImageFetcherForSimpleImage(/*valid_image=*/true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  PriceTrackingPageActionController controller(
      std::move(callback), shopping_service_.get(), image_fetcher_.get(),
      tracker_.get());

  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(12345L, GURL("http://example.com/image.png")));

  std::string histogram_name = "Commerce.PriceTracking.IconInteractionState";
  base::HistogramTester histogram_tester;

  // First, simulate a click on the expanded chip.
  controller.ResetForNewNavigation(GURL("http://example.com/1"));
  base::RunLoop().RunUntilIdle();
  controller.OnIconClicked();

  // Simulate no click on the chip.
  controller.ResetForNewNavigation(GURL("http://example.com/2"));
  base::RunLoop().RunUntilIdle();
  // Leave the page before a click happens.
  controller.ResetForNewNavigation(GURL("http://example.com/3"));
  base::RunLoop().RunUntilIdle();

  // Since the product is already tracked, we shouldn't have collected any
  // samples.
  histogram_tester.ExpectTotalCount(histogram_name, 0);
}

}  // namespace commerce
