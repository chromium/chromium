// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"

#include <functional>

#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

void LogStoredProfileCountStatistics(AutofillProfileRecordTypeCategory category,
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
}

void LogStoredProfileDaysSinceLastUse(
    AutofillProfileRecordTypeCategory category,
    size_t days) {
  base::UmaHistogramCounts1000(
      base::StrCat({"Autofill.DaysSinceLastUse.StoredProfile.",
                    GetProfileCategorySuffix(category)}),
      days);
}

void LogStoredProfileMetrics(
    const std::vector<const AutofillProfile*>& profiles) {
  const base::Time now = AutofillClock::Now();
  // Counts stored profile metrics for all profile of the given `category` and
  // emits UMA metrics for them.
  auto count_and_log = [&](AutofillProfileRecordTypeCategory category) {
    StoredProfileCounts counts;
    for (const AutofillProfile* profile : profiles) {
      if (category != GetCategoryOfProfile(*profile)) {
        continue;
      }
      const base::TimeDelta time_since_last_use = now - profile->use_date();
      LogStoredProfileDaysSinceLastUse(category, time_since_last_use.InDays());
      counts.total++;
      counts.disused += time_since_last_use > kDisusedDataModelTimeDelta;
    }
    LogStoredProfileCountStatistics(category, counts);
  };

  count_and_log(AutofillProfileRecordTypeCategory::kLocalOrSyncable);
  count_and_log(AutofillProfileRecordTypeCategory::kAccountChrome);
  count_and_log(AutofillProfileRecordTypeCategory::kAccountNonChrome);
  base::UmaHistogramCounts1M("Autofill.StoredProfileCount.Total",
                             profiles.size());
}

void LogLocalProfileSupersetMetrics(
    std::vector<const AutofillProfile*> profiles,
    std::string_view app_locale) {
  // Place all local profiles before all account profiles.
  std::vector<const AutofillProfile*>::iterator begin_account_profiles =
      base::ranges::partition(profiles,
                              std::not_fn(&AutofillProfile::IsAccountProfile));
  // Determines if a given `profile` is a strict superset of any account
  // profile.
  auto is_account_superset = [&, comparator =
                                     AutofillProfileComparator(app_locale)](
                                 const AutofillProfile* profile) {
    return std::ranges::any_of(begin_account_profiles, profiles.end(),
                               [&](const AutofillProfile* account_profile) {
                                 return profile->IsStrictSupersetOf(
                                     comparator, *account_profile);
                               });
  };
  // Count the number of local profiles which are a superset of some account
  // profile.
  base::UmaHistogramCounts100(
      "Autofill.Leipzig.Duplication.NumberOfLocalSupersetProfilesOnStartup",
      base::ranges::count_if(profiles.begin(), begin_account_profiles,
                             [&](const AutofillProfile* local_profile) {
                               return is_account_superset(local_profile);
                             }));
}

}  // namespace autofill::autofill_metrics
