// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_snooze_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {
BASE_FEATURE(kTestIPHFeature,
             "TestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestIPHFeature2,
             "TestIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);

class TestFeaturePromoSnoozeService : public FeaturePromoSnoozeService {
 public:
  TestFeaturePromoSnoozeService() = default;
  ~TestFeaturePromoSnoozeService() override = default;

  void Reset(const base::Feature& iph_feature) override {
    snooze_data_.erase(&iph_feature);
  }

  absl::optional<FeaturePromoSnoozeService::SnoozeData> ReadSnoozeData(
      const base::Feature& iph_feature) override {
    const auto it = snooze_data_.find(&iph_feature);
    return it == snooze_data_.end() ? absl::nullopt
                                    : absl::make_optional(it->second);
  }

  void SaveSnoozeData(const base::Feature& iph_feature,
                      const SnoozeData& snooze_data) override {
    snooze_data_[&iph_feature] = snooze_data;
  }

 private:
  std::map<const base::Feature*, SnoozeData> snooze_data_;
};

}  // namespace

class FeaturePromoSnoozeServiceTest : public testing::Test {
 public:
  FeaturePromoSnoozeServiceTest()
      : task_environment_{
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME} {}

 protected:
  base::test::TaskEnvironment task_environment_;
  TestFeaturePromoSnoozeService service_;
};

TEST_F(FeaturePromoSnoozeServiceTest, AllowFirstTimeIPH) {
  service_.Reset(kTestIPHFeature);
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, BlockDismissedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserDismiss(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  service_.Reset(kTestIPHFeature);
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, BlockSnoozedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, ReleaseSnoozedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature, base::Hours(1));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, MultipleIPH) {
  service_.Reset(kTestIPHFeature);
  service_.Reset(kTestIPHFeature2);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature, base::Hours(1));
  service_.OnPromoShown(kTestIPHFeature2);
  service_.OnUserSnooze(kTestIPHFeature2, base::Hours(3));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature2));
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature2));
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature2));
}

TEST_F(FeaturePromoSnoozeServiceTest, SnoozeNonClicker) {
  base::test::ScopedFeatureList feature_list;
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::Days(15));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

}  // namespace user_education
