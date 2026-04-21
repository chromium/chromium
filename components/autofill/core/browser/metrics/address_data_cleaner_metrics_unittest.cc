// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/address_data_cleaner_metrics.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_cleaner.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
// Checks that LogNumberOfProfilesConsideredForDedupePerCountryCode() counts the
// number of profiles per country code correctly. The function handles the case
// in which no country code is specified.
TEST(AddressDataCleanerMetricsTest,
     LogNumberOfProfilesConsideredForDedupePerCountryCode) {
  // Create profiles with different country codes.
  // 2 profiles from the US, 1 from CA, 1 from PL and 3 with missing country
  // code ("").
  using ProfileWithAction = AddressDataCleaner::ProfileWithAction;
  std::vector<AddressDataCleaner::ProfileWithAction> profiles = {
      ProfileWithAction{AutofillProfile(AddressCountryCode("US"))},
      ProfileWithAction{AutofillProfile(AddressCountryCode("US"))},
      ProfileWithAction{AutofillProfile(AddressCountryCode("CA"))},
      ProfileWithAction{AutofillProfile(AddressCountryCode("PL"))},
      ProfileWithAction{AutofillProfile(AddressCountryCode(""))},
      ProfileWithAction{AutofillProfile(AddressCountryCode(""))},
      ProfileWithAction{AutofillProfile(AddressCountryCode(""))}};

  base::HistogramTester histogram_tester;
  autofill_metrics::LogNumberOfProfilesConsideredForDedupePerCountryCode(
      profiles);

  // Verify the "CountryMissing" histogram.
  // There are 3 profiles with empty country, so we expect exactly 1 sample in
  // the bucket for '3'.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesWithMissingCountryCodeConsideredForDedupe",
      /*sample=*/3,
      /*expected_bucket_count=*/1);

  // Verify the standard "ByCountry" histogram.
  // We expect one sample of '2' (for "US"), one sample of '1' (for "PL") and
  // one sample of '1' (for "CA").
  histogram_tester.ExpectBucketCount(
      "Autofill.NumberOfProfilesPerValidCountryCodeConsideredForDedupe",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Autofill.NumberOfProfilesPerValidCountryCodeConsideredForDedupe",
      /*sample=*/1,
      /*expected_count=*/2);

  // Ensure no other unexpected country counts were logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.NumberOfProfilesPerValidCountryCodeConsideredForDedupe", 3);
}

}  // namespace
}  // namespace autofill
