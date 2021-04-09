// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
base::Feature kTestIPHFeature{"TestIPHFeature",
                              base::FEATURE_ENABLED_BY_DEFAULT};
base::Feature kTestIPHFeature2{"TestIPHFeature2",
                               base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace

class FeaturePromoSnoozeServiceTest : public testing::Test {
 public:
  FeaturePromoSnoozeServiceTest()
      : task_environment_{base::test::SingleThreadTaskEnvironment::TimeSource::
                              MOCK_TIME},
        service_{&profile_} {}

  void SetNonClickerPolicy(base::test::ScopedFeatureList& feature_list,
                           FeaturePromoSnoozeService::NonClickerPolicy policy) {
    std::map<std::string, std::string> parameters = {
        {"x_iph_snooze_non_clicker_policy",
         policy == FeaturePromoSnoozeService::NonClickerPolicy::kDismiss
             ? "dismiss"
             : "long_snooze"}};
    feature_list.InitAndEnableFeatureWithParameters(kTestIPHFeature,
                                                    parameters);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FeaturePromoSnoozeService service_;
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
  service_.OnUserSnooze(kTestIPHFeature, base::TimeDelta::FromHours(1));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, MultipleIPH) {
  service_.Reset(kTestIPHFeature);
  service_.Reset(kTestIPHFeature2);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature, base::TimeDelta::FromHours(1));
  service_.OnPromoShown(kTestIPHFeature2);
  service_.OnUserSnooze(kTestIPHFeature2, base::TimeDelta::FromHours(3));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature2));
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature2));
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature2));
}

TEST_F(FeaturePromoSnoozeServiceTest, SnoozeNonClicker) {
  base::test::ScopedFeatureList feature_list;
  SetNonClickerPolicy(feature_list,
                      FeaturePromoSnoozeService::NonClickerPolicy::kLongSnooze);
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(15));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, DismissNonClicker) {
  base::test::ScopedFeatureList feature_list;
  SetNonClickerPolicy(feature_list,
                      FeaturePromoSnoozeService::NonClickerPolicy::kDismiss);
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(15));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
}
