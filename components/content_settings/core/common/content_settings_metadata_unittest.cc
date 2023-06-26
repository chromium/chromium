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
        []() { return base::Time::FromDoubleT(12345); }, nullptr, nullptr);
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
                  /*expiration=*/base::Time::FromDoubleT(23456)));

    // The lifetime is returned directly, if provided.
    EXPECT_EQ(base::Seconds(10),
              RuleMetaData::ComputeLifetime(
                  /*lifetime=*/base::Seconds(10),
                  /*expiration=*/base::Time::FromDoubleT(23456)));
  }

  {
    base::subtle::ScopedTimeClockOverrides clock_overrides(
        []() { return base::Time::FromDoubleT(23456); }, nullptr, nullptr);
    // If the expiration is in the past, the computed lifetime is non-zero but
    // short.
    EXPECT_EQ(base::Microseconds(1),
              RuleMetaData::ComputeLifetime(
                  /*lifetime=*/base::TimeDelta(),
                  /*expiration=*/base::Time::FromDoubleT(12345)));
  }
}

TEST(RuleMetaDataTest, ComputeMetadata_Invalid) {
  // Supplying a lifetime but no expiration defers to the expiration.
  EXPECT_EQ(base::Seconds(0), RuleMetaData::ComputeLifetime(
                                  /*lifetime=*/base::Seconds(10),
                                  /*expiration=*/base::Time()));
}

}  // namespace content_settings
