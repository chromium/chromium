// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/new_badge_controller.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_storage_service.h"
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
  static constexpr int kDefaultStorageCap = 5;

  MockNewBadgePolicy() = default;
  ~MockNewBadgePolicy() override = default;

  MOCK_METHOD(bool,
              ShouldShowNewBadge,
              (const NewBadgeData&, const FeaturePromoStorageService&),
              (const, override));
  MOCK_METHOD(void,
              RecordNewBadgeShown,
              (const base::Feature&, int),
              (override));
  MOCK_METHOD(void, RecordFeatureUsed, (const base::Feature&, int), (override));

  int GetFeatureUsedStorageCap() const override { return mock_storage_cap_; }
  void set_mock_storage_cap(int new_cap) { mock_storage_cap_ = new_cap; }

 private:
  int mock_storage_cap_ = kDefaultStorageCap;
};

// A `NewBadgePolicy` that allows direct setting of the parameters.
class TestNewBadgePolicy : public NewBadgePolicy {
 public:
  TestNewBadgePolicy(int times_shown_before_dismiss,
                     int uses_before_dismiss,
                     base::TimeDelta show_window,
                     base::TimeDelta new_profile_grace_period)
      : NewBadgePolicy(times_shown_before_dismiss,
                       uses_before_dismiss,
                       show_window,
                       new_profile_grace_period) {}
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
  static constexpr base::TimeDelta kGracePeriod = base::Hours(8);
  static constexpr base::TimeDelta kDefaultProfileAge = base::Days(7);

  void SetUp() override {
    registry_.RegisterFeature({kNewBadgeTestFeature, Metadata()});
    storage_service_.set_clock_for_testing(&test_clock_);
    test_clock_.SetNow(base::Time::Now());
    storage_service_.set_profile_creation_time_for_testing(test_clock_.Now() -
                                                           kDefaultProfileAge);
  }

  void CreateWithTestPolicy(bool enable_feature = true) {
    if (enable_feature) {
      feature_list_.InitAndEnableFeature(kNewBadgeTestFeature);
    }
    auto policy = std::make_unique<TestNewBadgePolicy>(
        kMaxShows, kMaxUsed, kShowWindow, kGracePeriod);
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
  base::HistogramTester histogram_tester_;
};

TEST_F(NewBadgeControllerTest, MaybeShowNewBadgeCallsPolicyReturnsTrue) {
  CreateWithMockPolicy();
  EXPECT_CALL(*mock_policy_, ShouldShowNewBadge)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_policy_,
              RecordNewBadgeShown(testing::Ref(kNewBadgeTestFeature), 1));
  EXPECT_TRUE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  CheckData(kNewBadgeTestFeature, NewBadgeData{1, 0});
}

TEST_F(NewBadgeControllerTest, MaybeShowNewBadgeCallsPolicyReturnsFalse) {
  CreateWithMockPolicy();
  EXPECT_CALL(*mock_policy_, ShouldShowNewBadge)
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
  EXPECT_CALL(*mock_policy_,
              RecordFeatureUsed(testing::Ref(kNewBadgeTestFeature), 1));
  controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 1});
}

TEST_F(NewBadgeControllerTest, NotifyFeatureUsedIfValidIsValid) {
  CreateWithMockPolicy();
  EXPECT_CALL(*mock_policy_,
              RecordFeatureUsed(testing::Ref(kNewBadgeTestFeature), 1));
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

TEST_F(NewBadgeControllerTest, NotifyFeatureUsedDoesNotRecord) {
  constexpr int kStorageCap = 2;

  CreateWithMockPolicy();
  mock_policy_->set_mock_storage_cap(kStorageCap);

  for (int i = 0; i <= kStorageCap; ++i) {
    // The count passed to RecordFeatureUsed will top out just above the cap;
    // this isn't a problem for histograms, because they stop recording at the
    // max used count, which is (by design) lower than the storage cap.
    const int expected_count = std::min(i, kStorageCap) + 1;
    EXPECT_CALL(
        *mock_policy_,
        RecordFeatureUsed(testing::Ref(kNewBadgeTestFeature), expected_count));
    controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
  }

  // The number stored in prefs won't grow past the cap, however.
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, kStorageCap});
}

TEST_F(NewBadgeControllerTest, DoesNotShowIfFeatureDisabled) {
  CreateWithTestPolicy(/*enable_feature=*/false);
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 0});
}

TEST_F(NewBadgeControllerTest, DoesNotShowIfFeatureEnabledDuringGracePeriod) {
  CreateWithTestPolicy();
  storage_service_.set_profile_creation_time_for_testing(
      storage_service_.GetCurrentTime() - base::Hours(1));
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  CheckData(kNewBadgeTestFeature, NewBadgeData{0, 0});

  // Even if time is advanced, because the feature launched during the grace
  // period, it does not show a "New" Badge.
  test_clock_.Advance(base::Hours(8));
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
}

TEST_F(NewBadgeControllerTest, ShowsUntilMaxCount) {
  CreateWithTestPolicy();
  for (int i = 1; i <= kMaxShows + 3; ++i) {
    EXPECT_EQ(i <= kMaxShows,
              controller_->MaybeShowNewBadge(kNewBadgeTestFeature))
        << "On show #" << i;
    histogram_tester_.ExpectBucketCount(
        "UserEducation.NewBadge.NewBadgeTestFeature.MaxShownReached", false,
        std::min(i, kMaxShows - 1));
    histogram_tester_.ExpectBucketCount(
        "UserEducation.NewBadge.NewBadgeTestFeature.MaxShownReached", true,
        i >= kMaxShows ? 1 : 0);
  }
  CheckData(kNewBadgeTestFeature, NewBadgeData{kMaxShows, 0});
}

TEST_F(NewBadgeControllerTest, ShowsUntilMaxUsed) {
  CreateWithTestPolicy();
  for (int i = 0; i < kMaxUsed - 1; ++i) {
    controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
    histogram_tester_.ExpectBucketCount(
        "UserEducation.NewBadge.NewBadgeTestFeature.MaxUsedReached", false,
        i + 1);
    histogram_tester_.ExpectBucketCount(
        "UserEducation.NewBadge.NewBadgeTestFeature.MaxUsedReached", true, 0);
  }
  EXPECT_TRUE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  controller_->NotifyFeatureUsed(kNewBadgeTestFeature);

  // Verify both histograms are recorded.
  histogram_tester_.ExpectBucketCount(
      "UserEducation.NewBadge.NewBadgeTestFeature.MaxUsedReached", false,
      kMaxUsed - 1);
  histogram_tester_.ExpectBucketCount(
      "UserEducation.NewBadge.NewBadgeTestFeature.MaxUsedReached", true, 1);
  histogram_tester_.ExpectBucketCount(
      "UserEducation.NewBadge.NewBadgeTestFeature.MaxShownReached", false, 1);

  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  controller_->NotifyFeatureUsed(kNewBadgeTestFeature);
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));

  // Verify no changes in histograms.
  histogram_tester_.ExpectBucketCount(
      "UserEducation.NewBadge.NewBadgeTestFeature.MaxUsedReached", false,
      kMaxUsed - 1);
  histogram_tester_.ExpectBucketCount(
      "UserEducation.NewBadge.NewBadgeTestFeature.MaxUsedReached", true, 1);
  histogram_tester_.ExpectBucketCount(
      "UserEducation.NewBadge.NewBadgeTestFeature.MaxShownReached", false, 1);

  CheckData(kNewBadgeTestFeature, NewBadgeData{1, kMaxUsed + 1});
}

TEST_F(NewBadgeControllerTest, WindowBlocksBadge) {
  CreateWithTestPolicy();
  test_clock_.Advance(kShowWindow - base::Minutes(5));
  EXPECT_TRUE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));
  test_clock_.Advance(base::Minutes(10));
  EXPECT_FALSE(controller_->MaybeShowNewBadge(kNewBadgeTestFeature));

  // Verify that histogram is only recorded for the first show.
  histogram_tester_.ExpectBucketCount(
      "UserEducation.NewBadge.NewBadgeTestFeature.MaxShownReached", false, 1);
}

}  // namespace user_education
