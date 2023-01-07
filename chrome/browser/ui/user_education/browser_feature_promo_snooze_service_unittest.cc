// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/browser_feature_promo_snooze_service.h"

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
BASE_FEATURE(kTestIPHFeature,
             "TestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestIPHFeature2,
             "TestIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// Repeats some of the tests in FeaturePromoSnoozeServiceTest except that a live
// test profile is used to back the service instead of a dummy data map.
class BrowserFeaturePromoSnoozeServiceTest : public testing::Test {
 public:
  BrowserFeaturePromoSnoozeServiceTest()
      : task_environment_{base::test::SingleThreadTaskEnvironment::TimeSource::
                              MOCK_TIME},
        service_{&profile_} {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  BrowserFeaturePromoSnoozeService service_;
};

TEST_F(BrowserFeaturePromoSnoozeServiceTest, AllowFirstTimeIPH) {
  service_.Reset(kTestIPHFeature);
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoSnoozeServiceTest, BlockDismissedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserDismiss(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  service_.Reset(kTestIPHFeature);
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoSnoozeServiceTest, BlockSnoozedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoSnoozeServiceTest, ReleaseSnoozedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature, base::Hours(1));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoSnoozeServiceTest, MultipleIPH) {
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

TEST_F(BrowserFeaturePromoSnoozeServiceTest, SnoozeNonClicker) {
  base::test::ScopedFeatureList feature_list;
  service_.Reset(kTestIPHFeature);
  service_.OnPromoShown(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::Days(15));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}
