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
#include "components/autofill/core/browser/address_data_cleaner.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_features.h"

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
  // TODO(crbug.com/325452461): Implement more metrics.
}

void LogPercentageOfNonQuasiDuplicates(
    const std::vector<int>& profile_duplication_ranks) {
  CHECK(!profile_duplication_ranks.empty());

  for (int rank = 1; rank <= 5; rank++) {
    // Find the number of profiles which duplication rank is greater than
    // `rank`.
    const int number_of_duplicates_with_higher_rank = std::ranges::count_if(
        profile_duplication_ranks, [rank](int num) { return num > rank; });
    const double percentage_of_duplicates =
        (number_of_duplicates_with_higher_rank * 100.0) /
        profile_duplication_ranks.size();
    base::UmaHistogramPercentage(
        base::StrCat({kStartupHistogramPrefix,
                      "PercentageOfNonQuasiDuplicates.",
                      base::NumberToString(rank)}),
        percentage_of_duplicates);
  }
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
  if (profiles.size() > 100) {
    // Computing the metrics is quadratic in the number of profiles. To avoid
    // startup time regressions, these metrics are restricted to users with at
    // most 100 profiles (which covers the vast majority of users).
    return;
  }
  AutofillProfileComparator comparator(app_locale);
  std::vector<int> profile_duplicaton_ranks;
  for (const AutofillProfile* profile : profiles) {
    std::vector<FieldTypeSet> min_incompatible_sets =
        AddressDataCleaner::CalculateMinimalIncompatibleTypeSets(
            *profile, profiles, comparator);
    profile_duplicaton_ranks.push_back(
        GetDuplicationRank(min_incompatible_sets));
    LogDeduplicationStartupMetricsForProfile(*profile, min_incompatible_sets);
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillLogDeduplicationMetricsFollowup)) {
    LogPercentageOfNonQuasiDuplicates(profile_duplicaton_ranks);
  }
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
  // TODO(crbug.com/325452461): Implement more metrics.
}

}  // namespace autofill::autofill_metrics
