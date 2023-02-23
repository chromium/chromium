// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// Separate stored profile count metrics exist for every profile category. Test
// them in a parameterized way.
class StoredProfileMetricsTest
    : public testing::TestWithParam<AutofillProfileSourceCategory> {
 public:
  StoredProfileMetricsTest() {
    // Metrics for kAccount profiles are only emitted when the union view is
    // enabled, since kAccount profiles are otherwise not loaded.
    features_.InitAndEnableFeature(features::kAutofillAccountProfilesUnionView);
  }

  AutofillProfileSourceCategory Category() const { return GetParam(); }

  // Returns the suffix used for the metrics.
  std::string GetSuffix() const { return GetProfileCategorySuffix(Category()); }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    StoredProfileMetricsTest,
    testing::ValuesIn({AutofillProfileSourceCategory::kLocalOrSyncable,
                       AutofillProfileSourceCategory::kAccountChrome,
                       AutofillProfileSourceCategory::kAccountNonChrome}));

// Tests that no profile count metrics for the corresponding category are
// emitted when no profiles of that category are stored.
TEST_P(StoredProfileMetricsTest, NoProfiles) {
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

  // The following metric is only collected for kLocalOrSyncable profiles.
  if (Category() == AutofillProfileSourceCategory::kLocalOrSyncable) {
    histogram_tester.ExpectTotalCount(
        "Autofill.StoredProfileWithoutCountryCount", 0);
  }
}

// Tests that when profiles of a category exist, they metrics are emitted.
TEST_P(StoredProfileMetricsTest, StoredProfiles) {
  // Create a recently used (3 days ago) profile.
  AutofillProfile profile0 = test::GetFullProfile();
  profile0.set_use_date(AutofillClock::Now() - base::Days(3));
  test::SetProfileCategory(profile0, Category());

  // Create a profile used a long time (200 days) ago without a country.
  AutofillProfile profile1 = test::GetFullProfile2();
  profile1.ClearFields({ADDRESS_HOME_COUNTRY});
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

  // The following metric is only collected for kLocalOrSyncable profiles.
  if (Category() == AutofillProfileSourceCategory::kLocalOrSyncable) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.StoredProfileWithoutCountryCount", 1, 1);
  }
}

}  // namespace autofill::autofill_metrics
