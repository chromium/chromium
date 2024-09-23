// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_metadata.h"

#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

TEST(RuleMetaDataTest, ComputeMetadata) {
  {
    base::subtle::ScopedTimeClockOverrides clock_overrides(
        []() { return base::Time::FromSecondsSinceUnixEpoch(12345); }, nullptr,
        nullptr);
    // When no lifetime and expiration are provided, the result should also be
    // "empty".
    EXPECT_EQ(base::TimeDelta(), RuleMetaData::ComputeLifetime(
                                     /*lifetime=*/base::TimeDelta(),
                                     /*expiration=*/base::Time()));

    // The lifetime is computed conservatively when no explicit lifetime is
    // provided.
    EXPECT_EQ(base::Seconds(11111),
              RuleMetaData::ComputeLifetime(
                  /*lifetime=*/base::TimeDelta(),
                  /*expiration=*/base::Time::FromSecondsSinceUnixEpoch(23456)));

    // The lifetime is returned directly, if provided.
    EXPECT_EQ(base::Seconds(10),
              RuleMetaData::ComputeLifetime(
                  /*lifetime=*/base::Seconds(10),
                  /*expiration=*/base::Time::FromSecondsSinceUnixEpoch(23456)));
  }

  {
    base::subtle::ScopedTimeClockOverrides clock_overrides(
        []() { return base::Time::FromSecondsSinceUnixEpoch(23456); }, nullptr,
        nullptr);
    // If the expiration is in the past, the computed lifetime is non-zero but
    // short.
    EXPECT_EQ(base::Microseconds(1),
              RuleMetaData::ComputeLifetime(
                  /*lifetime=*/base::TimeDelta(),
                  /*expiration=*/base::Time::FromSecondsSinceUnixEpoch(12345)));
  }
}

TEST(RuleMetaDataTest, ComputeMetadata_Invalid) {
  // Supplying a lifetime but no expiration defers to the expiration.
  EXPECT_EQ(base::Seconds(0), RuleMetaData::ComputeLifetime(
                                  /*lifetime=*/base::Seconds(10),
                                  /*expiration=*/base::Time()));
}

TEST(RuleMetaDataTest, DefaultConstructor) {
  RuleMetaData metadata;
  EXPECT_EQ(metadata.last_modified(), base::Time());
  EXPECT_EQ(metadata.last_used(), base::Time());
  EXPECT_EQ(metadata.last_visited(), base::Time());
  EXPECT_EQ(metadata.expiration(), base::Time());
  EXPECT_EQ(metadata.session_model(), mojom::SessionModel::DURABLE);
  EXPECT_EQ(metadata.lifetime(), base::TimeDelta());
  EXPECT_FALSE(metadata.decided_by_related_website_sets());
}

TEST(RuleMetaDataTest, SetFromConstraints) {
  {
    ContentSettingConstraints constraints(
        base::Time::FromSecondsSinceUnixEpoch(12345));
    constraints.set_session_model(
        mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
    constraints.set_lifetime(base::Days(10));
    constraints.set_decided_by_related_website_sets(true);

    RuleMetaData metadata;
    metadata.SetFromConstraints(constraints);

    EXPECT_EQ(metadata.session_model(),
              mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
    EXPECT_EQ(metadata.expiration(),
              base::Time::FromSecondsSinceUnixEpoch(12345) + base::Days(10));
    EXPECT_EQ(metadata.lifetime(), base::Days(10));
    EXPECT_EQ(metadata.decided_by_related_website_sets(), true);
  }
}

TEST(RuleMetaDataTest, SetExpirationAndLifetime) {
  const base::Time now = base::Time::Now();
  RuleMetaData metadata;
  metadata.SetExpirationAndLifetime(now + base::Days(1), base::Days(2));
  EXPECT_EQ(metadata.expiration(), now + base::Days(1));
  EXPECT_EQ(metadata.lifetime(), base::Days(2));
}

}  // namespace content_settings
