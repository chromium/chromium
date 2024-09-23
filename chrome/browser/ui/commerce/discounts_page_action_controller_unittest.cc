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
const char kShoppingURLDomain[] = "example.com";
}

class DiscountsPageActionControllerUnittest : public testing::Test {
 public:
  DiscountsPageActionControllerUnittest()
      : shopping_service_(std::make_unique<MockShoppingService>()) {}

  void SetupDiscountResponseForURL(GURL url) {
    double expiry_time_sec =
        (base::DefaultClock::GetInstance()->Now() + base::Days(2))
            .InSecondsFSinceUnixEpoch();

    shopping_service_->SetResponseForGetDiscountInfoForUrl(
        {commerce::CreateValidDiscountInfo(
            /*detail=*/"Get 10% off",
            /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", /*discount_code=*/"discount_code",
            /*id=*/123,
            /*is_merchant_wide=*/true, expiry_time_sec)});
  }

  base::test::FeatureRefAndParams GetNoAutoShownBubbleParam() {
    return {commerce::kDiscountDialogAutoPopupBehaviorSetting,
            {{commerce::kMerchantWideBehaviorParam, "2"},
             {commerce::kNonMerchantWideBehaviorParam, "2"}}};
  }

  base::test::FeatureRefAndParams GetAlwaysAutoShownBubbleParam() {
    return {commerce::kDiscountDialogAutoPopupBehaviorSetting,
            {{commerce::kMerchantWideBehaviorParam, "1"},
             {commerce::kNonMerchantWideBehaviorParam, "1"}}};
  }

  base::test::FeatureRefAndParams GetAutoShownOnceBubbleParam() {
    return {commerce::kDiscountDialogAutoPopupBehaviorSetting,
            {{commerce::kMerchantWideBehaviorParam, "0"},
             {commerce::kNonMerchantWideBehaviorParam, "0"}}};
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
              GetDiscountInfoForUrl(GURL(kShoppingURL), testing::_));
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
              GetDiscountInfoForUrl(GURL(kShoppingURL), testing::_));
  // Empty response, hence no discounts.
  shopping_service_->SetResponseForGetDiscountInfoForUrl({});

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

  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrl(GURL(kShoppingURL), testing::_))
      .Times(0);

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.ShouldShowForNavigation().has_value());
  EXPECT_FALSE(controller.ShouldShowForNavigation().value());
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldExpandIcon_ShoppyPageOff) {
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.WantsExpandedUi());

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrl(GURL(kShoppingURL), testing::_));
  SetupDiscountResponseForURL(GURL(kShoppingURL));

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.WantsExpandedUi());
}

TEST_F(DiscountsPageActionControllerUnittest,
       ShouldExpandIcon_ShoppyPageOn_OnNonVisitedDomain) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableDiscountInfoApi, {{kDiscountOnShoppyPageParam, "true"}});

  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.WantsExpandedUi());

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrl(GURL(kShoppingURL), testing::_));
  SetupDiscountResponseForURL(GURL(kShoppingURL));

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.WantsExpandedUi());
  DiscountsPageActionController::DiscountsShownData* shown_data =
      static_cast<DiscountsPageActionController::DiscountsShownData*>(
          shopping_service_->GetUserData(
              DiscountsPageActionController::kDiscountsShownDataKey));

  EXPECT_THAT(shown_data->discount_shown_on_domains,
              testing::Contains(kShoppingURLDomain));
}

TEST_F(DiscountsPageActionControllerUnittest,
       ShouldExpandIcon_ShoppyPageOn_OnBubbleAutoShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kEnableDiscountInfoApi, {{kDiscountOnShoppyPageParam, "true"}}},
       GetAlwaysAutoShownBubbleParam()},
      /*disabled_features=*/{});

  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());

  // Simulate kShoppingURL has been visited before.
  DiscountsPageActionController::DiscountsShownData* shown_data =
      DiscountsPageActionController::GetOrCreate(shopping_service_.get());
  shown_data->discount_shown_on_domains.insert(kShoppingURLDomain);

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.WantsExpandedUi());

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrl(GURL(kShoppingURL), testing::_));
  SetupDiscountResponseForURL(GURL(kShoppingURL));

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller.WantsExpandedUi());
}

TEST_F(DiscountsPageActionControllerUnittest,
       ShouldNotExpandIcon_ShoppyPageOn_OnVisitedDomain) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableDiscountInfoApi, {{kDiscountOnShoppyPageParam, "true"}});

  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());

  // Simulate kShoppingURL has been visited before.
  DiscountsPageActionController::DiscountsShownData* shown_data =
      DiscountsPageActionController::GetOrCreate(shopping_service_.get());
  shown_data->discount_shown_on_domains.insert(kShoppingURLDomain);

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller.WantsExpandedUi());

  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));
  EXPECT_CALL(*shopping_service_,
              GetDiscountInfoForUrl(GURL(kShoppingURL), testing::_));
  SetupDiscountResponseForURL(GURL(kShoppingURL));

  // Simulate navigation to |kShoppingURL|
  controller.ResetForNewNavigation(GURL(kShoppingURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(controller.WantsExpandedUi());
}

TEST_F(DiscountsPageActionControllerUnittest, ShouldNotAutoShow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {GetNoAutoShownBubbleParam()},
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
      {GetAlwaysAutoShownBubbleParam()},
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
  constexpr uint64_t discount_id_1 = 123;
  constexpr uint64_t discount_id_2 = 456;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {GetAutoShownOnceBubbleParam()},
      /*disabled_features=*/{});
  shopping_service_->SetIsDiscountEligibleToShowOnNavigation(true);
  base::RepeatingCallback<void()> callback = notify_host_callback_.Get();

  DiscountsPageActionController controller(callback, shopping_service_.get());
  EXPECT_TRUE(controller.ShouldAutoShowBubble(discount_id_1,
                                              /*is_merchant_wide=*/false));
  EXPECT_TRUE(controller.ShouldAutoShowBubble(discount_id_2,
                                              /*is_merchant_wide=*/true));
  // Simulate |discount_id_1| and |discount_id_2| has been shown.
  controller.DiscountsBubbleShown(discount_id_1);
  controller.DiscountsBubbleShown(discount_id_2);

  EXPECT_FALSE(controller.ShouldAutoShowBubble(discount_id_1,
                                               /*is_merchant_wide=*/false));
  EXPECT_FALSE(controller.ShouldAutoShowBubble(discount_id_2,
                                               /*is_merchant_wide=*/true));
}

}  // namespace commerce
