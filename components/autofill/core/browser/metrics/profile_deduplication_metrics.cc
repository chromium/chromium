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
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/levenshtein_distance.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/address_data_cleaner.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

namespace {

constexpr std::string_view kStartupHistogramPrefix =
    "Autofill.Deduplication.ExistingProfiles.";
constexpr std::string_view kImportHistogramPrefix =
    "Autofill.Deduplication.NewProfile.";

int CalculateQualityScoreForProfile(
    base::span<const ProfileTokenQuality::ObservationType>
        profile_observations) {
  const auto [profile_good_observations, profile_bad_observations] =
      AddressDataCleaner::CountObservationsByQualityForDeduplicationPurposes(
          profile_observations);
  return static_cast<int>(profile_good_observations) -
         static_cast<int>(profile_bad_observations);
}

// Logs the types that prevent a profile from being a duplicate, if its
// `duplication_rank` is sufficiently low (i.e. not many conflicting types).
void LogTypeOfQuasiDuplicateTokenMetric(
    std::string_view metric_name_prefix,
    int duplication_rank,
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets) {
  const std::string metric_name =
      base::StrCat({metric_name_prefix, "TypeOfQuasiDuplicateToken.",
                    base::NumberToString(duplication_rank)});
  for (const DifferingProfileWithTypeSet& types : min_incompatible_sets) {
    for (FieldType type : types.field_type_set) {
      base::UmaHistogramEnumeration(
          metric_name, ConvertSettingsVisibleFieldTypeForMetrics(type));
    }
  }
}

void LogEditingDistanceOfQuasiDuplicateToken(
    const AutofillProfile& profile,
    const int profile_duplication_rank,
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets,
    const std::string& app_locale) {
  for (auto& [other_profile, set] : min_incompatible_sets) {
    for (FieldType type : set) {
      // It could be that the normalization increases or decreases the editing
      // distance. It is needed to calculate both(normalized and not) editing
      // distances and take the minimum. Eg:
      // Streat vs Street: 1
      // after normalization:
      // Streat vs Str:  >1
      // or
      // Street vs Strr: >1
      // after normalization:
      // Str vs Strr: 1
      const size_t raw_distance =
          base::LevenshteinDistance(profile.GetInfo(type, app_locale),
                                    other_profile->GetInfo(type, app_locale));
      const size_t normalized_distance = base::LevenshteinDistance(
          NormalizeAndRewrite(profile.GetAddressCountryCode(),
                              profile.GetInfo(type, app_locale),
                              /*keep_white_space=*/false),
          NormalizeAndRewrite(other_profile->GetAddressCountryCode(),
                              other_profile->GetInfo(type, app_locale),
                              /*keep_white_space=*/false));

      base::UmaHistogramCounts1000(
          base::StrCat({"Autofill.Deduplication.ExistingProfiles."
                        "EditingDistanceOfQuasiDuplicateToken.",
                        base::ToString(profile_duplication_rank), ".",
                        FieldTypeToStringView(type)}),
          std::min(raw_distance, normalized_distance));
    }
  }
}

// If `is_import` is true, then only existing profiles are used for quality
// score calculations. That happens because at the import moment imported
// profile does not have any observations.
void LogQualityOfQuasiDuplicateTokenMetric(
    std::string_view metric_name_prefix,
    const AutofillProfile& profile,
    int duplication_rank,
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets,
    bool is_import) {
  for (const auto& [other_profile, types] : min_incompatible_sets) {
    for (FieldType type : types) {
      if ((!is_import && profile.token_quality()
                             .GetObservationTypesForFieldType(type)
                             .empty()) ||
          other_profile->token_quality()
              .GetObservationTypesForFieldType(type)
              .empty()) {
        continue;
      }

      // There is currently no clean way to record ranges that include negative
      // numbers in UMA. To address that this histogram will record values from
      // 0 to 20(inclusive) where:
      // [0, 9] - mean score in range [-10, -1]
      // 10 - mean score 0
      // [11 - 20] - mean score in range [1, 10]
      // This can be achieved by adding 10 to the score(since the minimal value
      // a score can have is -10).
      static_assert(ProfileTokenQuality::kMaxObservationsPerToken == 10);
      int score;
      if (is_import) {
        score = CalculateQualityScoreForProfile(
            other_profile->token_quality().GetObservationTypesForFieldType(
                type));
      } else {
        score = std::min(
            CalculateQualityScoreForProfile(
                profile.token_quality().GetObservationTypesForFieldType(type)),
            CalculateQualityScoreForProfile(
                other_profile->token_quality().GetObservationTypesForFieldType(
                    type)));
      }
      const std::string metric_name =
          base::StrCat({metric_name_prefix, "QualityOfQuasiDuplicateToken.",
                        base::NumberToString(duplication_rank), ".",
                        FieldTypeToStringView(type)});
      base::UmaHistogramExactLinear(metric_name, 10 + score,
                                    /*exclusive_max=*/21);
    }
  }
}

void LogQuasiDuplicateAdoption(
    const AutofillProfile& profile,
    int duplication_rank,
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets) {
  for (const auto& [other_profile, types] : min_incompatible_sets) {
    const size_t score =
        std::min(profile.use_count(), other_profile->use_count());
    const size_t total_use_count =
        profile.use_count() + other_profile->use_count();
    // This metric is recording a pair (score, total_use_count), where both
    // values are capped such that they fit in range [0, 99] and then encoded
    // such that 8 lowest bits are represent the capped total_use_count, the
    // next 8 bits represent the capped score.
    const size_t histogram_value = std::min<size_t>(score, 99) << 8 |
                                   std::min<size_t>(total_use_count, 99);
    base::UmaHistogramSparse(
        base::StrCat(
            {"Autofill.Deduplication.ExistingProfiles.QuasiDuplicateAdoption.",
             base::NumberToString(duplication_rank), ".QualityThreshold"}),
        histogram_value);
  }
}

void LogDeduplicationStartupMetricsForProfile(
    const AutofillProfile& profile,
    base::span<const DifferingProfileWithTypeSet>
        min_incompatible_differing_sets,
    std::string_view app_locale) {
  const int duplication_rank =
      GetDuplicationRank(min_incompatible_differing_sets);
  if (duplication_rank < 1 || duplication_rank > 5) {
    return;
  }
  base::UmaHistogramCounts100(
      base::StrCat(
          {kStartupHistogramPrefix, "RankOfStoredQuasiDuplicateProfiles"}),
      duplication_rank);
  LogTypeOfQuasiDuplicateTokenMetric(kStartupHistogramPrefix, duplication_rank,
                                     min_incompatible_differing_sets);

  if (base::FeatureList::IsEnabled(
          features::kAutofillLogDeduplicationMetricsFollowup)) {
    // TODO(crbug.com/325452461): Implement more metrics.
    LogEditingDistanceOfQuasiDuplicateToken(profile, duplication_rank,
                                            min_incompatible_differing_sets,
                                            std::string(app_locale));
    LogQualityOfQuasiDuplicateTokenMetric(
        kStartupHistogramPrefix, profile, duplication_rank,
        min_incompatible_differing_sets, /*is_import=*/false);
    LogQuasiDuplicateAdoption(profile, duplication_rank,
                              min_incompatible_differing_sets);
  }
}

void LogPercentageOfNonQuasiDuplicates(
    const std::vector<int>& profile_duplication_ranks) {
  CHECK(!profile_duplication_ranks.empty());

  for (int rank = 0; rank <= 5; rank++) {
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

int GetDuplicationRank(
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets) {
  // All elements of `min_incompatible_sets` have the same size.
  return min_incompatible_sets.empty()
             ? std::numeric_limits<int>::max()
             : min_incompatible_sets.back().field_type_set.size();
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
    std::vector<DifferingProfileWithTypeSet>
        min_incompatible_differential_sets =
            AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
                *profile, profiles, comparator);
    profile_duplicaton_ranks.push_back(
        GetDuplicationRank(min_incompatible_differential_sets));
    LogDeduplicationStartupMetricsForProfile(
        *profile, min_incompatible_differential_sets, app_locale);
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
  std::vector<DifferingProfileWithTypeSet> min_incompatible_sets =
      AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
          import_candidate, existing_profiles,
          AutofillProfileComparator(app_locale));
  const int duplication_rank = GetDuplicationRank(min_incompatible_sets);
  if (duplication_rank < 1 || duplication_rank > 5) {
    return;
  }

  // Emit the actual metrics, based on the user decision.
  const std::string metric_name_prefix = base::StrCat(
      {kImportHistogramPrefix, did_user_accept ? "Accepted" : "Declined", "."});
  base::UmaHistogramCounts100(
      base::StrCat({metric_name_prefix, "RankOfStoredQuasiDuplicateProfiles"}),
      duplication_rank);
  LogTypeOfQuasiDuplicateTokenMetric(metric_name_prefix, duplication_rank,
                                     min_incompatible_sets);
  if (base::FeatureList::IsEnabled(
          features::kAutofillLogDeduplicationMetricsFollowup)) {
    LogQualityOfQuasiDuplicateTokenMetric(
        metric_name_prefix, import_candidate, duplication_rank,
        min_incompatible_sets, /*is_import=*/true);
  }
}

}  // namespace autofill::autofill_metrics
