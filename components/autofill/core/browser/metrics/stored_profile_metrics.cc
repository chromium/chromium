// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

void LogStoredProfileCountStatistics(AutofillProfileSourceCategory category,
                                     const StoredProfileCounts& counts) {
  const std::string kSuffix = GetProfileCategorySuffix(category);

  base::UmaHistogramCounts1M("Autofill.StoredProfileCount." + kSuffix,
                             counts.total);
  // For users without any profiles do not record the other metrics.
  if (counts.total == 0) {
    return;
  }
  DCHECK_LE(counts.disused, counts.total);
  size_t used = counts.total - counts.disused;
  base::UmaHistogramCounts1000("Autofill.StoredProfileUsedCount." + kSuffix,
                               used);
  base::UmaHistogramCounts1000("Autofill.StoredProfileDisusedCount." + kSuffix,
                               counts.disused);
  base::UmaHistogramPercentage(
      "Autofill.StoredProfileUsedPercentage." + kSuffix,
      100 * used / counts.total);
  // `kAccount` profiles are guaranteed to have a country, so this metric is
  // only tracked for the `kLocalOrSyncable` category. For this reason `kSuffix`
  // is not applied to the metrics name either.
  if (category == AutofillProfileSourceCategory::kLocalOrSyncable) {
    base::UmaHistogramCounts1M("Autofill.StoredProfileWithoutCountryCount",
                               counts.without_country);
  }
}

void LogStoredProfileDaysSinceLastUse(AutofillProfileSourceCategory category,
                                      size_t days) {
  base::UmaHistogramCounts1000(
      base::StrCat({"Autofill.DaysSinceLastUse.StoredProfile.",
                    GetProfileCategorySuffix(category)}),
      days);
}

void LogStoredProfileMetrics(const std::vector<AutofillProfile*>& profiles) {
  const base::Time now = AutofillClock::Now();
  // Counts stored profile metrics for all profile of the given `category` and
  // emits UMA metrics for them.
  auto count_and_log = [&](AutofillProfileSourceCategory category) {
    StoredProfileCounts counts;
    for (const AutofillProfile* profile : profiles) {
      if (category != GetCategoryOfProfile(*profile)) {
        continue;
      }
      const base::TimeDelta time_since_last_use = now - profile->use_date();
      LogStoredProfileDaysSinceLastUse(category, time_since_last_use.InDays());
      counts.total++;
      counts.disused += time_since_last_use > kDisusedDataModelTimeDelta;
      counts.without_country += profile->HasRawInfo(ADDRESS_HOME_COUNTRY);
    }
    LogStoredProfileCountStatistics(category, counts);
  };

  count_and_log(AutofillProfileSourceCategory::kLocalOrSyncable);
  // These metrics are only relevant when kAccount profiles are loaded.
  if (base::FeatureList::IsEnabled(
          features::kAutofillAccountProfilesUnionView)) {
    count_and_log(AutofillProfileSourceCategory::kAccountChrome);
    count_and_log(AutofillProfileSourceCategory::kAccountNonChrome);
  }
}

}  // namespace autofill::autofill_metrics
