// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
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

constexpr std::string_view kStartupHistogramPrefix =
    "Autofill.Deduplication.ExistingProfiles.";
constexpr std::string_view kImportHistogramPrefix =
    "Autofill.Deduplication.NewProfile.";

// Logs the types that prevent a profile from being a duplicate, if its
// `duplication_rank` is sufficiently low (i.e. not many conflicting types).
void LogTypeOfQuasiDuplicateTokenMetric(
    std::string_view metric_name_prefix,
    int duplication_rank,
    base::span<const FieldTypeSet> min_incompatible_sets) {
  if (duplication_rank < 1 || duplication_rank > 5) {
    return;
  }
  const std::string metric_name =
      base::StrCat({metric_name_prefix, "TypeOfQuasiDuplicateToken.",
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
    base::span<const FieldTypeSet> min_incompatible_sets) {
  const int duplication_rank = GetDuplicationRank(min_incompatible_sets);
  base::UmaHistogramCounts100(
      base::StrCat(
          {kStartupHistogramPrefix, "RankOfStoredQuasiDuplicateProfiles"}),
      duplication_rank);
  LogTypeOfQuasiDuplicateTokenMetric(kStartupHistogramPrefix, duplication_rank,
                                     min_incompatible_sets);
  // TODO(b/325452461): Implement more metrics.
}

}  // namespace

int GetDuplicationRank(base::span<const FieldTypeSet> min_incompatible_sets) {
  // All elements of `min_incompatible_sets` have the same size.
  return min_incompatible_sets.empty() ? std::numeric_limits<int>::max()
                                       : min_incompatible_sets.back().size();
}

void LogDeduplicationStartupMetrics(
    base::span<const AutofillProfile* const> profiles,
    std::string_view app_locale) {
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
  profiles_copy.reserve(profiles.size());
  for (const AutofillProfile* profile : profiles) {
    profiles_copy.push_back(*profile);
  }
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(log_metrics, std::move(profiles_copy),
                                std::string(app_locale)));
}

void LogDeduplicationImportMetrics(
    bool did_user_accept,
    const AutofillProfile& import_candidate,
    base::span<const AutofillProfile* const> existing_profiles,
    std::string_view app_locale) {
  DCHECK(!base::Contains(
      existing_profiles, import_candidate.guid(),
      [](const AutofillProfile* profile) { return profile->guid(); }));
  if (existing_profiles.empty()) {
    // Don't pollute metrics with cases where obviously no duplicates exists.
    return;
  }

  // Calculate the `duplication_rank`.
  std::vector<FieldTypeSet> min_incompatible_sets =
      AddressDataCleaner::CalculateMinimalIncompatibleTypeSets(
          import_candidate, existing_profiles,
          AutofillProfileComparator(app_locale));
  const int duplication_rank = GetDuplicationRank(min_incompatible_sets);

  // Emit the actual metrics, based on the user decision.
  const std::string metric_name_prefix = base::StrCat(
      {kImportHistogramPrefix, did_user_accept ? "Accepted" : "Declined", "."});
  base::UmaHistogramCounts100(
      base::StrCat({metric_name_prefix, "RankOfStoredQuasiDuplicateProfiles"}),
      duplication_rank);
  LogTypeOfQuasiDuplicateTokenMetric(metric_name_prefix, duplication_rank,
                                     min_incompatible_sets);
  // TODO(b/325452461): Implement more metrics.
}

}  // namespace autofill::autofill_metrics
