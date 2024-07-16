// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/discounts_page_action_controller.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "url/gurl.h"

namespace commerce {

namespace {
const char kShoppingURL[] = "https://example.com";
}

class DiscountsPageActionControllerUnittest : public testing::Test {
 public:
  DiscountsPageActionControllerUnittest()
      : shopping_service_(std::make_unique<MockShoppingService>()) {}

  void SetupDiscountResponseForURL(GURL url) {
    double expiry_time_sec =
        (base::DefaultClock::GetInstance()->Now() + base::Days(2))
            .InSecondsFSinceUnixEpoch();

    shopping_service_->SetResponseForGetDiscountInfoForUrls(
        {{url,
          {commerce::CreateValidDiscountInfo(
              /*detail=*/"Get 10% off",
              /*terms_and_conditions=*/"",
              /*value_in_text=*/"$10 off", /*discount_code=*/"discount_code",
              /*id=*/123,
              /*is_merchant_wide=*/true, expiry_time_sec)}}});
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::MockRepeatingCallback<void()> notify_host_callback_;
  std::unique_ptr<MockShoppingService> shopping_service_;
};

TEST_F(DiscountsPageActionControllerUnittest, ShouldShowIcon) {
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.ShouldShowForNavigation().has_value());

  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrls(std::vector<GURL>{GURL(kShoppingURL)},
                                     testing::_));
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  SetupDiscountResponseForURL(GURL(kShoppingURL));

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.ShouldShowForNavigation().has_value());
  EXPECT_TRUE(controller.ShouldShowForNavigation().value());
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldNotShowIcon_NoDiscounts) {
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.ShouldShowForNavigation().has_value());

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrls(std::vector<GURL>{GURL(kShoppingURL)},
                                     testing::_));
  // Empty response, hence no discounts.
  shopping_service_->SetResponseForGetDiscountInfoForUrls({});

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.ShouldShowForNavigation().has_value());
  EXPECT_FALSE(controller.ShouldShowForNavigation().value());
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldNotShowIcon_NoEligible) {
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(false);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  DiscountsPageActionController controller(callback, shopping_service_.get());

  EXPECT_CALL(
      *shopping_service_,
      GetDiscountInfoForUrls(std::vector<GURL>{GURL(kShoppingURL)}, testing::_))
      .Times(0);

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.ShouldShowForNavigation().has_value());
  EXPECT_FALSE(controller.ShouldShowForNavigation().value());
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldExpandIcon) {
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.WantsExpandedUi());

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrls(std::vector<GURL>{GURL(kShoppingURL)},
                                     testing::_));
  SetupDiscountResponseForURL(GURL(kShoppingURL));

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.WantsExpandedUi());
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldNotAutoShow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{commerce::kDiscountDialogAutoPopupBehaviorSetting,
        {{commerce::kMerchantWideBehaviorParam, "2"},
         {commerce::kNonMerchantWideBehaviorParam, "2"}}}},
      /*disabled_features=*/{});
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());
  EXPECT_FALSE(controller.ShouldAutoShowBubble(/*discount_id=*/123,
                                               /*is_merchant_wide=*/false));
  EXPECT_FALSE(controller.ShouldAutoShowBubble(/*discount_id=*/456,
                                               /*is_merchant_wide=*/true));
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldAlwaysAutoShow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{commerce::kDiscountDialogAutoPopupBehaviorSetting,
        {{commerce::kMerchantWideBehaviorParam, "1"},
         {commerce::kNonMerchantWideBehaviorParam, "1"}}}},
      /*disabled_features=*/{});
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());
  EXPECT_TRUE(controller.ShouldAutoShowBubble(/*discount_id=*/123,
                                              /*is_merchant_wide=*/false));
  EXPECT_TRUE(controller.ShouldAutoShowBubble(/*discount_id=*/456,
                                              /*is_merchant_wide=*/true));
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldAutoShowOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{commerce::kDiscountDialogAutoPopupBehaviorSetting,
        {{commerce::kMerchantWideBehaviorParam, "0"},
         {commerce::kNonMerchantWideBehaviorParam, "0"}}}},
      /*disabled_features=*/{});
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
  EXPECT_CALL(*shopping_service_, HasDiscountShownBefore(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Return(false));
  ;

  DiscountsPageActionController controller(callback, shopping_service_.get());
  EXPECT_TRUE(controller.ShouldAutoShowBubble(/*discount_id=*/123,
                                              /*is_merchant_wide=*/false));
  EXPECT_TRUE(controller.ShouldAutoShowBubble(/*discount_id=*/456,
                                              /*is_merchant_wide=*/true));

  EXPECT_CALL(*shopping_service_, HasDiscountShownBefore(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Return(true));
  ;
  EXPECT_FALSE(controller.ShouldAutoShowBubble(/*discount_id=*/123,
                                               /*is_merchant_wide=*/false));
  EXPECT_FALSE(controller.ShouldAutoShowBubble(/*discount_id=*/456,
                                               /*is_merchant_wide=*/true));
}

}  // namespace commerce
