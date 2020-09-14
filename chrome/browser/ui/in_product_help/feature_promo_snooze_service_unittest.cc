// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/feature_promo_snooze_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FeaturePromoSnoozeService service_;
};

TEST_F(FeaturePromoSnoozeServiceTest, AllowNonSnoozedIPH) {
  service_.Reset(kTestIPHFeature);
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, BlockDismissedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnUserDismiss(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  service_.Reset(kTestIPHFeature);
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, BlockSnoozedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature);
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, ReleaseSnoozedIPH) {
  service_.Reset(kTestIPHFeature);
  service_.OnUserSnooze(kTestIPHFeature, base::TimeDelta::FromHours(1));
  EXPECT_TRUE(service_.IsBlocked(kTestIPHFeature));
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));
  EXPECT_FALSE(service_.IsBlocked(kTestIPHFeature));
}

TEST_F(FeaturePromoSnoozeServiceTest, MultipleIPH) {
  service_.Reset(kTestIPHFeature);
  service_.Reset(kTestIPHFeature2);
  service_.OnUserSnooze(kTestIPHFeature, base::TimeDelta::FromHours(1));
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
