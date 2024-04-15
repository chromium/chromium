// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

constexpr char kLocale[] = "en_US";

class ProfileDeduplicationMetricsTest : public testing::Test {
 public:
  // Startup metrics are computed asynchronously. Used to wait for the result.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ProfileDeduplicationMetricsTest, RankOfStoredQuasiDuplicateProfiles) {
  // Create a pair of profiles with duplication rank 2.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"different company");
  b.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  base::HistogramTester histogram_tester;
  LogDeduplicationStartupMetrics({&a, &b}, kLocale);
  RunUntilIdle();
  // The same sample is emitted once for each profile.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      2, 2);
}

// Tests that when the user doesn't have other profiles, no metrics are emitted.
TEST_F(ProfileDeduplicationMetricsTest,
       RankOfStoredQuasiDuplicateProfiles_NoProfiles) {
  AutofillProfile a = test::GetFullProfile();
  base::HistogramTester histogram_tester;
  LogDeduplicationStartupMetrics({&a}, kLocale);
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      0);
}

}  // namespace

}  // namespace autofill::autofill_metrics
