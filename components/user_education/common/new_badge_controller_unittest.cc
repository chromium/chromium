// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/new_badge_controller.h"

#include <optional>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/new_badge_policy.h"
#include "components/user_education/common/new_badge_specification.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/test/test_feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {

BASE_FEATURE(kNewBadgeTestFeature,
             "NewBadgeTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOtherTestFeature,
             "OtherTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Mock for testing `NewBadgeController` without a live `NewBadgePolicy`.
class MockNewBadgePolicy : public NewBadgePolicy {
 public:
  MockNewBadgePolicy() = default;
  ~MockNewBadgePolicy() override = default;

  MOCK_METHOD(bool,
              ShouldShowNewBadge,
              (const base::Feature&, int, int, base::TimeDelta),
              (const, override));
};

// A `NewBadgePolicy` that allows direct setting of the parameters.
class TestNewBadgePolicy : public NewBadgePolicy {
 public:
  TestNewBadgePolicy(int times_shown_before_dismiss,
                     int uses_before_dismiss,
                     base::TimeDelta show_window)
      : NewBadgePolicy(times_shown_before_dismiss,
                       uses_before_dismiss,
                       show_window) {}
  ~TestNewBadgePolicy() override = default;
};

}  // namespace

class NewBadgeControllerTest : public testing::Test {
 public:
  NewBadgeControllerTest() = default;
  ~NewBadgeControllerTest() override = default;

  static constexpr int kMaxShows = 5;
  static constexpr int kMaxUsed = 2;
  static constexpr base::TimeDelta kShowWindow = base::Days(1);

  void SetUp() override {
    registry_.RegisterFeature({kNewBadgeTestFeature, Metadata()});
    storage_service_.set_clock_for_testing(&test_clock_);
    test_clock_.SetNow(base::Time::Now());
  }

  void CreateWithTestPolicy(bool enable_feature = true) {
    if (enable_feature) {
      feature_list_.InitAndEnableFeature(kNewBadgeTestFeature);
    }
    auto policy =
        std::make_unique<TestNewBadgePolicy>(kMaxShows, kMaxUsed, kShowWindow);
    controller_ = std::make_unique<NewBadgeController>(
        registry_, storage_service_, std::move(policy));
    controller_->InitData();
  }

  void CreateWithMockPolicy(bool enable_feature = true) {
    if (enable_feature) {
      feature_list_.InitAndEnableFeature(kNewBadgeTestFeature);
    }
    auto policy = std::make_unique<testing::StrictMock<MockNewBadgePolicy>>();
    mock_policy_ = policy.get();
    controller_ = std::make_unique<NewBadgeController>(
        registry_, storage_service_, std::move(policy));
    controller_->InitData();
  }

  void CheckData(const base::Feature& feature, const NewBadgeData& expected) {
    const auto actual = storage_service_.ReadNewBadgeData(feature);
    EXPECT_EQ(expected.show_count, actual.show_count);
    EXPECT_EQ(expected.used_count, actual.used_count);
  }

 protected:
  base::SimpleTestClock test_clock_;
  NewBadgeRegistry registry_;
  test::TestFeaturePromoStorageService storage_service_;
  std::unique_ptr<NewBadgeController> controller_;
  raw_ptr<MockNewBadgePolicy> mock_policy_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NewBadgeControllerTest, MaybeShowNewBadgeCallsPolicyReturnsTrue) {
  CreateWithMockPolicy();
  EXPECT_CALL(
      *mock_policy_,
      ShouldShowNewBadge(testing::Ref(kNewBadgeTestFeature), 0, 0, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  CheckData(kNewBadgeTestFeature, NewBadgeData{1, 0});
}

TEST_F(NewBadgeControllerTest, MaybeShowNewBadgeCallsPolicyReturnsFalse) {
  CreateWithMockPolicy();
  EXPECT_CALL(
      *mock_policy_,
      ShouldShowNewBadge(testing::Ref(kNewBadgeTestFeature), 0, 0, testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 0});
}

TEST_F(NewBadgeControllerTest, NewBadgesDisabledForTesting) {
  auto lock = NewBadgeController::DisableNewBadgesForTesting();
  CreateWithMockPolicy();
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 0});
}

TEST_F(NewBadgeControllerTest, NotifyFeatureUsed) {
  CreateWithMockPolicy();
  controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 1});
}

TEST_F(NewBadgeControllerTest, NotifyFeatureUsedIfValidIsValid) {
  CreateWithMockPolicy();
  controller_->NotifyFeatureUsedIfValid(kNewBadgeTestFeature);
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 1});
}

TEST_F(NewBadgeControllerTest, NotifyFeatureUsedIfValidNotRegistered) {
  CreateWithMockPolicy();
  controller_->NotifyFeatureUsedIfValid(kOtherTestFeature);
  CheckData(kOtherTestFeature, NewBadgeData{0, 0});
}

TEST_F(NewBadgeControllerTest, NotifyFeatureUsedIfValidNotEnabled) {
  CreateWithMockPolicy(false);
  controller_->NotifyFeatureUsedIfValid(kNewBadgeTestFeature);
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 0});
}

TEST_F(NewBadgeControllerTest, DoesNotShowIfFeatureDisabled) {
  CreateWithTestPolicy(/*enable_feature=*/false);
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 0});
}

TEST_F(NewBadgeControllerTest, ShowsUntilMaxCount) {
  CreateWithTestPolicy();
  for (int i = 1; i <= kMaxShows + 3; ++i) {
    EXPECT_EQ(i <= kMaxShows,
              controller_->MaybeShowNewBadge(kNewBadgeTestFeature))
        << "On show #" << i;
  }
  CheckData(kNewBadgeTestFeature, NewBadgeData{kMaxShows, 0});
}

TEST_F(NewBadgeControllerTest, ShowsUntilMaxUsed) {
  CreateWithTestPolicy();
  for (int i = 0; i < kMaxUsed - 1; ++i) {
    controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
  }
  EXPECT_TRUE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));

  CheckData(kNewBadgeTestFeature, NewBadgeData{1, kMaxUsed + 1});
}

TEST_F(NewBadgeControllerTest, WindowBlocksBadge) {
  CreateWithTestPolicy();
  test_clock_.Advance(kShowWindow - base::Minutes(5));
  EXPECT_TRUE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  test_clock_.Advance(base::Minutes(10));
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
}

}  // namespace user_education
