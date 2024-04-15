// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/address_data_cleaner.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

namespace {

constexpr std::string_view kHistogramPrefix =
    "Autofill.Deduplication.ExistingProfiles.";

void LogTypeOfQuasiDuplicateTokenMetric(
    int duplication_rank,
    std::vector<FieldTypeSet> min_incompatible_sets) {
  if (duplication_rank < 1 || duplication_rank > 5) {
    return;
  }
  const std::string metric_name =
      base::StrCat({kHistogramPrefix, "TypeOfQuasiDuplicateToken.",
                    base::NumberToString(duplication_rank)});
  for (const FieldTypeSet& types : min_incompatible_sets) {
    for (FieldType type : types) {
      base::UmaHistogramEnumeration(
          metric_name, ConvertSettingsVisibleFieldTypeForMetrics(type));
    }
  }
}

void LogDeduplicationStartupMetricsForProfile(
    const AutofillProfile& profile,
    std::vector<FieldTypeSet> min_incompatible_sets) {
  // The minimum number of fields whose removal makes `profile` a duplicate.
  const int duplication_rank = min_incompatible_sets.empty()
                                   ? std::numeric_limits<int>::max()
                                   : min_incompatible_sets.back().size();

  base::UmaHistogramCounts100(
      base::StrCat({kHistogramPrefix, "RankOfStoredQuasiDuplicateProfiles"}),
      duplication_rank);
  LogTypeOfQuasiDuplicateTokenMetric(duplication_rank, min_incompatible_sets);
  // TODO(b/325452461): Implement more metrics.
}

}  // namespace

void LogDeduplicationStartupMetrics(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale) {
  if (profiles.size() <= 1) {
    // Don't pollute metrics with cases where obviously no duplicates exists.
    return;
  }
  auto log_metrics = [](std::vector<AutofillProfile> profiles,
                        const std::string& app_locale) {
    AutofillProfileComparator comparator(app_locale);
    for (AutofillProfile& profile : profiles) {
      LogDeduplicationStartupMetricsForProfile(
          profile, AddressDataCleaner::CalculateMinimalIncompatibleTypeSets(
                       profile, profiles, comparator));
    }
  };
  // Since computing the metrics is quadratic in `profiles.size()`, it is done
  // on a background thread. Create a copy of the `profiles`, to avoid passing
  // pointers between threads.
  std::vector<AutofillProfile> profiles_copy;
  for (const AutofillProfile* profile : profiles) {
    profiles_copy.push_back(*profile);
  }
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(log_metrics, std::move(profiles_copy), app_locale));
}

}  // namespace autofill::autofill_metrics
