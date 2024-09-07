// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

using VariationsServiceUtilsTest = ::testing::Test;

// Verifies that the seed expiration status is correctly computed based on its
// fetch time and the binary build time.
TEST(VariationsServiceUtilsTest, HasSeedExpiredSinceTime) {
  base::Time now = base::Time::Now();
  struct {
    const base::Time fetch_time;
    const base::Time build_time;
    const bool expired;
  } test_cases[] = {
      // Verifies that seed should NOT expire if it does NOT exceed the maximum
      // age permitted for a variations seed.
      {now - base::Days(30), now, false},
      // Verifies that when the binary is newer than the seed, the seed should
      // be considered expired if it exceeds maximum age permitted.
      {now - base::Days(31), now, true},
      // Verifies that when the binary is older than the seed, the seed should
      // NOT be considered expired even when it exceeds maximum age permitted.
      {now - base::Days(31), now - base::Days(32), false},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(HasSeedExpiredSinceTimeHelperForTesting(test.fetch_time,
                                                      test.build_time),
              test.expired);
  }
}
}  // namespace variations
