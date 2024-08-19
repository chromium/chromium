// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

constexpr char kLocale[] = "en_US";

TEST(ProfileDeduplicationMetricsTest,
     Startup_RankOfStoredQuasiDuplicateProfiles) {
  // Create a pair of profiles with duplication rank 2.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"different company");
  b.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> profiles = {&a, &b};
  LogDeduplicationStartupMetrics(profiles, kLocale);
  // The same sample is emitted once for each profile.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      2, 2);
}

// Tests that when the user doesn't have other profiles, no metrics are emitted.
TEST(ProfileDeduplicationMetricsTest,
     Startup_RankOfStoredQuasiDuplicateProfiles_NoProfiles) {
  AutofillProfile a = test::GetFullProfile();
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> profiles = {&a};
  LogDeduplicationStartupMetrics(profiles, kLocale);
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      0);
}

TEST(ProfileDeduplicationMetricsTest,
     Startup_PercentageOfNonQuasiDuplicates_NotEnoughProfiles) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillLogDeduplicationMetricsFollowup);
  AutofillProfile a = test::GetFullProfile();
  const std::vector<AutofillProfile*> profiles = {&a};
  base::HistogramTester histogram_tester;
  LogDeduplicationStartupMetrics(profiles, kLocale);
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.1",
      0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.2",
      0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.3",
      0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.4",
      0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.5",
      0);
}

TEST(ProfileDeduplicationMetricsTest, Startup_PercentageOfNonQuasiDuplicates) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLogDeduplicationMetricsFollowup);
  // Create 4 profiles, c and d have duplication rank 2.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  AutofillProfile c = test::GetFullProfile();
  c.SetRawInfo(COMPANY_NAME, u"different company");
  c.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  AutofillProfile d = test::GetFullProfile();
  d.SetRawInfo(COMPANY_NAME, u"testing company");
  d.SetRawInfo(EMAIL_ADDRESS, u"test-email@gmail.com");
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> profiles = {&a, &b, &c, &d};
  LogDeduplicationStartupMetrics(profiles, kLocale);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "1",
      50, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "2",
      0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "3",
      0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "4",
      0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "5",
      0, 1);
}

TEST(ProfileDeduplicationMetricsTest, Startup_TypeOfQuasiDuplicateToken) {
  // `a` differs from `b` and `c` only in a single type.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"different company");
  AutofillProfile c = test::GetFullProfile();
  c.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  base::HistogramTester histogram_tester;
  const std::vector<AutofillProfile*> profiles = {&a, &b, &c};
  LogDeduplicationStartupMetrics(profiles, kLocale);
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

TEST(ProfileDeduplicationMetricsTest,
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

TEST(ProfileDeduplicationMetricsTest, Import_TypeOfQuasiDuplicateToken) {
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
