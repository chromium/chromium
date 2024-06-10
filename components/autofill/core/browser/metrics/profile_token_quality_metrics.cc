// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"

#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

namespace {

constexpr std::string_view kHistogramPrefix = "Autofill.ProfileTokenQuality.";

using ObservationType = ProfileTokenQuality::ObservationType;

// Gets all types of the `profile` that are relevant for ProfileTokenQuality
// metrics. This excludes additional supported types, since no observations
// are tracked for them.
FieldTypeSet GetMetricRelevantTypes(const AutofillProfile& profile) {
  FieldTypeSet relevant_types;
  profile.GetSupportedTypes(&relevant_types);
  relevant_types.intersect(GetDatabaseStoredTypesOfAutofillProfile());
  return relevant_types;
}

// Returns the total number of observations for all `types`.
size_t GetTotalObservationCount(const AutofillProfile& profile,
                                const FieldTypeSet& types) {
  size_t total_observations = 0;
  for (FieldType type : types) {
    total_observations +=
        profile.token_quality().GetObservationTypesForFieldType(type).size();
  }
  return total_observations;
}

// Emits Autofill.ProfileTokenQuality.StoredObservationTypes.{Type}, for every
// Type in `types`. It tracks the different observation types available for that
// Type.
void LogStoredObservationsPerType(const AutofillProfile& profile,
                                  const FieldTypeSet& types) {
  for (FieldType type : types) {
    for (ObservationType observation :
         profile.token_quality().GetObservationTypesForFieldType(type)) {
      base::UmaHistogramEnumeration(
          base::StrCat({kHistogramPrefix, "StoredObservationTypes.",
                        FieldTypeToStringView(type)}),
          observation);
    }
  }
}

// For metrics purposes, to get a high-level overview of the token and profile
// quality, observations are classified as good, neutral and bad based on this
// function. The number of good and bad `observations` are returned.
std::pair<size_t, size_t> CountObservationsByQuality(
    const std::vector<ObservationType>& observations) {
  size_t good = 0, bad = 0;
  for (ObservationType observation : observations) {
    switch (observation) {
      case ObservationType::kAccepted:
      case ObservationType::kPartiallyAccepted:
        good++;
        break;
      case ObservationType::kEditedToDifferentTokenOfSameProfile:
      case ObservationType::kEditedToSameTokenOfOtherProfile:
      case ObservationType::kEditedToDifferentTokenOfOtherProfile:
      case ObservationType::kEditedFallback:
        bad++;
        break;
      case ObservationType::kUnknown:
      case ObservationType::kEditedToSimilarValue:
      case ObservationType::kEditedValueCleared:
        // Neutral observations types are not relevant for any metric.
        break;
    }
  }
  return {good, bad};
}

// Emits Autofill.ProfileTokenQuality.{Type} as the acceptance rate of all the
// Types in `types` (based on the observation quality defined by
// `CountObservationsByQuality()`).
// Also emits Autofill.ProfileTokenQuality.PerProfile, which represents the same
// acceptance rate, but accumulated over all `types`.
void LogStoredTokenQuality(const AutofillProfile& profile,
                           const FieldTypeSet& types) {
  size_t total_stored_good_observations = 0, total_stored_bad_observations = 0;
  for (FieldType type : types) {
    auto [good_observations, bad_observations] = CountObservationsByQuality(
        profile.token_quality().GetObservationTypesForFieldType(type));
    if (good_observations + bad_observations == 0) {
      continue;
    }
    base::UmaHistogramPercentage(
        base::StrCat({kHistogramPrefix, FieldTypeToStringView(type)}),
        100 * good_observations / (good_observations + bad_observations));
    total_stored_good_observations += good_observations;
    total_stored_bad_observations += bad_observations;
  }
  if (total_stored_good_observations + total_stored_bad_observations) {
    base::UmaHistogramPercentage(
        base::StrCat({kHistogramPrefix, "PerProfile"}),
        100 * total_stored_good_observations /
            (total_stored_good_observations + total_stored_bad_observations));
  }
}

// Calculates the quality score of observations based on
// `CountObservationsByQuality()`. The score is guaranteed to have values from 0
// to 10.
size_t CalculateQualityScore(const std::vector<ObservationType>& observations) {
  CHECK(observations.size() > 0);
  auto [good_observations, bad_observations] =
      CountObservationsByQuality(observations);
  // If only neutral observations exist, return a neutral score.
  if (good_observations + bad_observations == 0) {
    return 5;
  }
  return std::round(10.0 * good_observations /
                    (good_observations + bad_observations));
}

// This function encodes the integer value of `field_type`, `quality_score` and
// `n_observations` into a 16 bit integer. The lower four
// bits are used to encode `n_observations`, the following 4 bits for
// `quality_score` and the higher 8 bits are used to encode the field type.
std::optional<int> GetQualityScoreBucket(
    FieldType field_type,
    const std::vector<ObservationType>& observations) {
  static_assert(FieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 8),
                "Autofill::FieldType value needs more than 8 bits.");
  static_assert(
      ProfileTokenQuality::kMaxObservationsPerToken <= (UINT16_MAX >> 12),
      "ProfileTokenQuality::kMaxObservationsPerToken needs more than 4 bits.");
  size_t quality_score = CalculateQualityScore(observations);
  CHECK_LE(quality_score, 10UL);
  size_t n_observations = observations.size();
  if (n_observations < 1 || n_observations > 10) {
    return std::nullopt;
  }
  return (field_type << 8) | (static_cast<int>(quality_score) << 4) |
         static_cast<int>(n_observations);
}

}  // namespace

void LogStoredProfileTokenQualityMetrics(
    const std::vector<const AutofillProfile*>& profiles) {
  for (const AutofillProfile* profile : profiles) {
    FieldTypeSet relevant_types = GetMetricRelevantTypes(*profile);
    base::UmaHistogramCounts1000(
        base::StrCat({kHistogramPrefix, "StoredObservationsCount.PerProfile"}),
        GetTotalObservationCount(*profile, relevant_types));
    LogStoredObservationsPerType(*profile, relevant_types);
    LogStoredTokenQuality(*profile, relevant_types);
  }
}

void LogObservationCountBeforeSubmissionMetric(const FormStructure& form,
                                               const PersonalDataManager& pdm) {
  std::set<const AutofillProfile*> profiles_used;
  // Emit per-type metrics for all autofilled fields.
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (!field->autofill_source_profile_guid()) {
      // The field was not autofilled.
      continue;
    }
    if (const AutofillProfile* profile =
            pdm.address_data_manager().GetProfileByGUID(
                *field->autofill_source_profile_guid())) {
      profiles_used.insert(profile);
      FieldType field_type = field->Type().GetStorableType();
      base::UmaHistogramExactLinear(
          base::StrCat({kHistogramPrefix, "ObservationCountBeforeSubmission.",
                        FieldTypeToStringView(field_type)}),
          profile->token_quality()
              .GetObservationTypesForFieldType(field_type)
              .size(),
          ProfileTokenQuality::kMaxObservationsPerToken + 1);
    }
  }
  // Emit the total observation counts for all `profiles_used`.
  for (const AutofillProfile* profile : profiles_used) {
    base::UmaHistogramCounts1000(
        base::StrCat(
            {kHistogramPrefix, "ObservationCountBeforeSubmission.PerProfile"}),
        GetTotalObservationCount(*profile, GetMetricRelevantTypes(*profile)));
  }
}

void LogProfileTokenQualityScoreMetric(const FormStructure& form,
                                       const PersonalDataManager& pdm) {
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (!field->autofill_source_profile_guid()) {
      // The field was not autofilled.
      continue;
    }
    if (const AutofillProfile* profile =
            pdm.address_data_manager().GetProfileByGUID(
                *field->autofill_source_profile_guid())) {
      FieldTypeSet relevant_types = GetMetricRelevantTypes(*profile);
      FieldType field_type = field->Type().GetStorableType();
      if (!relevant_types.contains(field_type)) {
        continue;
      }
      std::vector<ObservationType> observations =
          profile->token_quality().GetObservationTypesForFieldType(field_type);
      if (observations.size() == 0) {
        continue;
      }
      std::optional<int> bucket =
          GetQualityScoreBucket(field_type, observations);
      if (bucket) {
        base::UmaHistogramSparse("Autofill.ProfileTokenQualityScore", *bucket);
      }
    }
  }
}

}  // namespace autofill::autofill_metrics
