// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_utils.h"

#include "base/test/metrics/histogram_tester.h"
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

TEST(VariationsServiceUtilsTest,
     EmitFutureSeedMetricsForSeedWithFutureFetchTime) {
  base::Time now = base::Time::Now();
  base::Time future_fetch_time = now + base::Hours(50);
  base::HistogramTester histogram_tester;
  HasSeedExpiredSinceTimeHelperForTesting(future_fetch_time,
                                          /*build_time=*/now);

  histogram_tester.ExpectUniqueSample("Variations.HasFutureSeed",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness.Future",
                                      /*sample=*/2,
                                      /*expected_bucket_count=*/1);
}

TEST(VariationsServiceUtilsTest,
     EmitFutureSeedMetricsForSeedWithPastFetchTime) {
  base::Time now = base::Time::Now();
  base::Time past_fetch_time = now - base::Days(5);
  base::HistogramTester histogram_tester;
  HasSeedExpiredSinceTimeHelperForTesting(past_fetch_time,
                                          /*build_time=*/now);

  histogram_tester.ExpectUniqueSample("Variations.HasFutureSeed",
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("Variations.SeedFreshness.Future",
                                    /*expected_count=*/0);
}
}  // namespace variations
