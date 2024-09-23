// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

constexpr char kLocale[] = "en_US";

class ProfileDeduplicationMetricsTest : public testing::Test {
 public:
  ProfileDeduplicationMetricsTest() = default;

 protected:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillLogDeduplicationMetricsFollowup};
};

TEST_F(ProfileDeduplicationMetricsTest,
       Startup_RankOfStoredQuasiDuplicateProfiles) {
  // Create a pair of profiles with duplication rank 2.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"different company");
  b.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  const std::vector<const AutofillProfile*> profiles = {&a, &b};

  LogDeduplicationStartupMetrics(profiles, kLocale);
  // The same sample is emitted once for each profile.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      2, 2);
}

// Tests that when the user doesn't have other profiles, no metrics are emitted.
TEST_F(ProfileDeduplicationMetricsTest,
       Startup_RankOfStoredQuasiDuplicateProfiles_NoProfiles) {
  AutofillProfile a = test::GetFullProfile();
  const std::vector<const AutofillProfile*> profiles = {&a};

  LogDeduplicationStartupMetrics(profiles, kLocale);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "RankOfStoredQuasiDuplicateProfiles",
      0);
}

TEST_F(ProfileDeduplicationMetricsTest,
       Startup_PercentageOfNonQuasiDuplicates_NotEnoughProfiles) {
  AutofillProfile a = test::GetFullProfile();
  const std::vector<const AutofillProfile*> profiles = {&a};

  LogDeduplicationStartupMetrics(profiles, kLocale);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.1",
      0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.2",
      0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.3",
      0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.4",
      0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Deduplication.ExistingProfiles."
      "PercentageOfNonQuasiDuplicates.5",
      0);
}

TEST_F(ProfileDeduplicationMetricsTest,
       Startup_PercentageOfNonQuasiDuplicates) {
  // Create 4 profiles, c and d have duplication rank 2.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  AutofillProfile c = test::GetFullProfile();
  c.SetRawInfo(COMPANY_NAME, u"different company");
  c.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  AutofillProfile d = test::GetFullProfile();
  d.SetRawInfo(COMPANY_NAME, u"testing company");
  d.SetRawInfo(EMAIL_ADDRESS, u"test-email@gmail.com");
  const std::vector<const AutofillProfile*> profiles = {&a, &b, &c, &d};

  LogDeduplicationStartupMetrics(profiles, kLocale);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "1",
      50, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "2",
      0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "3",
      0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "4",
      0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.PercentageOfNonQuasiDuplicates."
      "5",
      0, 1);
}

TEST_F(ProfileDeduplicationMetricsTest, Startup_EditDistance) {
  AutofillProfile a = test::GetFullProfile();
  a.SetRawInfo(COMPANY_NAME, u"A company");
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"B company");
  const std::vector<const AutofillProfile*> profiles = {&a, &b};
  LogDeduplicationStartupMetrics(profiles, kLocale);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles."
      "EditingDistanceOfQuasiDuplicateToken.1.COMPANY_NAME",
      1, 2);
}

TEST_F(ProfileDeduplicationMetricsTest, Startup_EditDistanceNormalized) {
  AutofillProfile a = test::GetFullProfile();
  a.SetRawInfo(COMPANY_NAME, u"Jean- François");
  AutofillProfile b = test::GetFullProfile();
  // The normalized distance should be 2 as only the suffix is different.
  b.SetRawInfo(COMPANY_NAME, u"jean francoiska");
  const std::vector<const AutofillProfile*> profiles = {&a, &b};
  LogDeduplicationStartupMetrics(profiles, kLocale);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles."
      "EditingDistanceOfQuasiDuplicateToken.1.COMPANY_NAME",
      2, 2);
}

TEST_F(ProfileDeduplicationMetricsTest, Startup_TypeOfQuasiDuplicateToken) {
  // `a` differs from `b` and `c` only in a single type.
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"different company");
  AutofillProfile c = test::GetFullProfile();
  c.SetRawInfo(EMAIL_ADDRESS, u"different-email@gmail.com");
  const std::vector<const AutofillProfile*> profiles = {&a, &b, &c};

  LogDeduplicationStartupMetrics(profiles, kLocale);
  // Expect two samples for `kCompany` and `kEmailAddress`, since:
  // - `kCompany` and `kEmailAddress` are each emitted once by `a`.
  // - `kCompany` is emitted once by `b`.
  // - `kEmailAddress` is emitted once by `c`.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Autofill.Deduplication.ExistingProfiles."
                                      "TypeOfQuasiDuplicateToken.1"),
      base::BucketsAre(
          base::Bucket(SettingsVisibleFieldTypeForMetrics::kCompany, 2),
          base::Bucket(SettingsVisibleFieldTypeForMetrics::kEmailAddress, 2)));
}

TEST_F(ProfileDeduplicationMetricsTest,
       Startup_QualityOfQuasiDuplicateTokenNegative) {
  AutofillProfile a = test::GetFullProfile();
  a.SetRawInfo(COMPANY_NAME, u"A Company");
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"B Company");

  // Profile `a` has 1 good observation, 0 bad ones.
  test_api(a.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);

  // Profile `b` has 1 good observation and 3 bad ones.
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);

  const std::vector<const AutofillProfile*> profiles = {&a, &b};
  LogDeduplicationStartupMetrics(profiles, kLocale);

  // Lower score = -2 + 10(offset) => 8(coming from the profile b) should be
  // recorded.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.QualityOfQuasiDuplicateToken.1."
      "COMPANY_NAME",
      8, 2);
}

TEST_F(ProfileDeduplicationMetricsTest,
       Startup_QualityOfQuasiDuplicateTokenPositive) {
  AutofillProfile a = test::GetFullProfile();
  a.SetRawInfo(COMPANY_NAME, u"A Company");
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"B Company");

  // Profile `a` has 1 good observation, 0 bad ones.
  test_api(a.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);

  // Profile `b` has 3 good observations and 1 bad one.
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(b.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);

  const std::vector<const AutofillProfile*> profiles = {&a, &b};
  LogDeduplicationStartupMetrics(profiles, kLocale);

  // Lower score = 1 + 10(offset) => 11 (coming from the profile a) should be
  // recorded.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.QualityOfQuasiDuplicateToken.1."
      "COMPANY_NAME",
      11, 2);
}

TEST_F(ProfileDeduplicationMetricsTest, Startup_QuasiDuplicateAdoption) {
  AutofillProfile a = test::GetFullProfile();
  a.SetRawInfo(COMPANY_NAME, u"A Company");
  AutofillProfile b = test::GetFullProfile();
  b.SetRawInfo(COMPANY_NAME, u"B Company");

  a.set_use_count(50);
  b.set_use_count(100);

  const std::vector<const AutofillProfile*> profiles = {&a, &b};
  LogDeduplicationStartupMetrics(profiles, kLocale);

  // Lower score is equal 50, the total count is 150, but all entries are capped
  // at 99. The score is shifted 8 bits left to create space for the total use
  // count.
  // |Lower score| |total use count|
  //     00110010  01100011
  // After converting back to decimal, it should be equal to 12899
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.ExistingProfiles.QuasiDuplicateAdoption.1."
      "QualityThreshold",
      12899, 2);
}

TEST_F(ProfileDeduplicationMetricsTest,
       Import_RankOfStoredQuasiDuplicateProfiles) {
  AutofillProfile existing_profile = test::GetFullProfile();
  // `import_candidate` has duplication rank 1 with `existing_profile`.
  AutofillProfile import_candidate = test::GetFullProfile();
  import_candidate.SetRawInfo(COMPANY_NAME, u"different company");
  const std::vector<const AutofillProfile*> existing_profiles = {
      &existing_profile};
  LogDeduplicationImportMetrics(/*did_user_accept=*/true, import_candidate,
                                existing_profiles, kLocale);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.NewProfile.Accepted."
      "RankOfStoredQuasiDuplicateProfiles",
      1, 1);
}

TEST_F(ProfileDeduplicationMetricsTest, Import_TypeOfQuasiDuplicateToken) {
  AutofillProfile existing_profile = test::GetFullProfile();
  // `import_candidate` has duplication rank 1 with `existing_profile`.
  AutofillProfile import_candidate = test::GetFullProfile();
  import_candidate.SetRawInfo(COMPANY_NAME, u"different company");
  const std::vector<const AutofillProfile*> existing_profiles = {
      &existing_profile};
  LogDeduplicationImportMetrics(/*did_user_accept=*/false, import_candidate,
                                existing_profiles, kLocale);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Autofill.Deduplication.NewProfile.Declined."
                  "TypeOfQuasiDuplicateToken.1"),
              base::BucketsAre(base::Bucket(
                  SettingsVisibleFieldTypeForMetrics::kCompany, 1)));
}

TEST_F(ProfileDeduplicationMetricsTest,
       Import_QualityOfQuasiDuplicateTokenNegative) {
  AutofillProfile existing_profile = test::GetFullProfile();
  // `import_candidate` has duplication rank 1 with `existing_profile`.
  AutofillProfile import_candidate = test::GetFullProfile();
  import_candidate.SetRawInfo(COMPANY_NAME, u"different company");

  // Existing profile has 1 good observation and 3 bad ones.
  test_api(existing_profile.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(existing_profile.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);
  test_api(existing_profile.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);
  test_api(existing_profile.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);

  const std::vector<const AutofillProfile*> existing_profiles = {
      &existing_profile};
  LogDeduplicationImportMetrics(/*did_user_accept=*/false, import_candidate,
                                existing_profiles, kLocale);

  // Score = -2  + 10 => 8 should be recorded.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.NewProfile.Declined.QualityOfQuasiDuplicateToken."
      "1."
      "COMPANY_NAME",
      8, 1);
}

TEST_F(ProfileDeduplicationMetricsTest,
       Import_QualityOfQuasiDuplicateTokenPositive) {
  AutofillProfile existing_profile = test::GetFullProfile();
  // `import_candidate` has duplication rank 1 with `existing_profile`.
  AutofillProfile import_candidate = test::GetFullProfile();
  import_candidate.SetRawInfo(COMPANY_NAME, u"different company");

  // Existing profile has 2 good observations and 1 bad one.
  test_api(existing_profile.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(existing_profile.token_quality())
      .AddObservation(COMPANY_NAME,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(existing_profile.token_quality())
      .AddObservation(COMPANY_NAME, ProfileTokenQuality::ObservationType::
                                        kEditedToDifferentTokenOfSameProfile);

  const std::vector<const AutofillProfile*> existing_profiles = {
      &existing_profile};
  LogDeduplicationImportMetrics(/*did_user_accept=*/false, import_candidate,
                                existing_profiles, kLocale);

  // Score = 1 + 10(offset) => 11 should be recorded.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Deduplication.NewProfile.Declined.QualityOfQuasiDuplicateToken."
      "1."
      "COMPANY_NAME",
      11, 1);
}

}  // namespace

}  // namespace autofill::autofill_metrics
