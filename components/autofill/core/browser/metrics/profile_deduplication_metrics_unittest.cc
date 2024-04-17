// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
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

TEST_F(ProfileDeduplicationMetricsTest,
       Startup_RankOfStoredQuasiDuplicateProfiles) {
  // Create a pair of profiles with duplication rank 2.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"different company");
  b.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> profiles = {&a, &b};
  LogDeduplicationStartupMetrics(profiles, kLocale);
  RunUntilIdle();
  // The same sample is emitted once for each profile.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      2, 2);
}

// Tests that when the user doesn't have other profiles, no metrics are emitted.
TEST_F(ProfileDeduplicationMetricsTest,
       Startup_RankOfStoredQuasiDuplicateProfiles_NoProfiles) {
  AutofillProfile a = test::GetFullProfile();
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> profiles = {&a};
  LogDeduplicationStartupMetrics(profiles, kLocale);
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      0);
}

TEST_F(ProfileDeduplicationMetricsTest, Startup_TypeOfQuasiDuplicateToken) {
  // `a` differs from `b` and `c` only in a single type.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"different company");
  AutofillProfile c = test::GetFullProfile();
  c.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> profiles = {&a, &b, &c};
  LogDeduplicationStartupMetrics(profiles, kLocale);
  RunUntilIdle();
  // Expect two samples for `kCompany` and `kEmailAddress`, since:
  // - `kCompany` and `kEmailAddress` are each emitted once by `a`.
  // - `kCompany` is emitted once by `b`.
  // - `kEmailAddress` is emitted once by `c`.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Deduplication.ExistingProfiles."
                                     "TypeOfQuasiDuplicateToken.1"),
      base::BucketsAre(
          base::Bucket(SettingsVisibleFieldTypeForMetrics::kCompany, 2),
          base::Bucket(SettingsVisibleFieldTypeForMetrics::kEmailAddress, 2)));
}

TEST_F(ProfileDeduplicationMetricsTest,
       Import_RankOfStoredQuasiDuplicateProfiles) {
  AutofillProfile existing_profile = test::GetFullProfile();
  // `import_candidate` has duplication rank 1 with `existing_profile`.
  AutofillProfile import_candidate = test::GetFullProfile();
  import_candidate.SetRawInfo(COMPANY_NAME, u"different company");
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> existing_profiles = {&existing_profile};
  LogDeduplicationImportMetrics(/*did_user_accept=*/true, import_candidate,
                                existing_profiles, kLocale);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.NewProfile.Accepted."
      "RankOfStoredQuasiDuplicateProfiles",
      1, 1);
}

TEST_F(ProfileDeduplicationMetricsTest, Import_TypeOfQuasiDuplicateToken) {
  AutofillProfile existing_profile = test::GetFullProfile();
  // `import_candidate` has duplication rank 1 with `existing_profile`.
  AutofillProfile import_candidate = test::GetFullProfile();
  import_candidate.SetRawInfo(COMPANY_NAME, u"different company");
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> existing_profiles = {&existing_profile};
  LogDeduplicationImportMetrics(/*did_user_accept=*/false, import_candidate,
                                existing_profiles, kLocale);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.Deduplication.NewProfile.Declined."
                  "TypeOfQuasiDuplicateToken.1"),
              base::BucketsAre(base::Bucket(
                  SettingsVisibleFieldTypeForMetrics::kCompany, 1)));
}

}  // namespace

}  // namespace autofill::autofill_metrics
