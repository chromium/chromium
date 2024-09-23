// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// Separate stored profile count metrics exist for every profile category. Test
// them in a parameterized way.
class StoredProfileMetricsTestByCategory
    : public testing::TestWithParam<AutofillProfileRecordTypeCategory> {
 public:
  StoredProfileMetricsTestByCategory() = default;

  AutofillProfileRecordTypeCategory Category() const { return GetParam(); }

  // Returns the suffix used for the metrics.
  std::string GetSuffix() const { return GetProfileCategorySuffix(Category()); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    StoredProfileMetricsTestByCategory,
    testing::ValuesIn({AutofillProfileRecordTypeCategory::kLocalOrSyncable,
                       AutofillProfileRecordTypeCategory::kAccountChrome,
                       AutofillProfileRecordTypeCategory::kAccountNonChrome}));

// Tests that no profile count metrics for the corresponding category are
// emitted when no profiles of that category are stored.
TEST_P(StoredProfileMetricsTestByCategory, NoProfiles) {
  base::HistogramTester histogram_tester;
  LogStoredProfileMetrics(/*profiles=*/{});

  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCount." + GetSuffix(), 0, 1);
  // The following metrics are expected not to be emitted when no profiles are
  // stored.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileDisusedCount." + GetSuffix(), 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileUsedCount." + GetSuffix(), 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileUsedPercentage." + GetSuffix(), 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredProfile." + GetSuffix(), 0);
}

// Tests that when profiles of a category exist, they metrics are emitted.
TEST_P(StoredProfileMetricsTestByCategory, StoredProfiles) {
  // Create a recently used (3 days ago) profile.
  AutofillProfile profile0 = test::GetFullProfile();
  profile0.set_use_date(AutofillClock::Now() - base::Days(3));
  test::SetProfileCategory(profile0, Category());

  // Create a profile used a long time (200 days) ago.
  AutofillProfile profile1 = test::GetFullProfile2();
  profile1.set_use_date(AutofillClock::Now() - base::Days(200));
  test::SetProfileCategory(profile1, Category());

  // Log the metrics and verify expectations.
  base::HistogramTester histogram_tester;
  LogStoredProfileMetrics({&profile0, &profile1});

  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCount." + GetSuffix(), 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileDisusedCount." + GetSuffix(), 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileUsedCount." + GetSuffix(), 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileUsedPercentage." + GetSuffix(), 50, 1);

  const std::string last_used_metric =
      "Autofill.DaysSinceLastUse.StoredProfile." + GetSuffix();
  histogram_tester.ExpectTotalCount(last_used_metric, 2);
  histogram_tester.ExpectBucketCount(last_used_metric, 3, 1);
  histogram_tester.ExpectBucketCount(last_used_metric, 200, 1);
}

// Tests that `LogLocalProfileSupersetMetrics()` determines the correct number
// of superset profiles.
TEST(StoredProfileMetricsTest, LocalProfileSupersetMetrics) {
  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  AutofillProfile local_profile1 = test::StandardProfile();
  AutofillProfile local_profile2 = test::SubsetOfStandardProfile();
  AutofillProfile local_profile3 = test::DifferentFromStandardProfile();

  base::HistogramTester histogram_tester;
  LogLocalProfileSupersetMetrics(
      {&account_profile, &local_profile1, &local_profile2, &local_profile3},
      "en-US");
  // Expect that `local_profile1` is a strict superset of `account_profile`, but
  // `local_profile2` and `local_profile3` is not.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Leipzig.Duplication.NumberOfLocalSupersetProfilesOnStartup", 1,
      1);
}

}  // namespace autofill::autofill_metrics
