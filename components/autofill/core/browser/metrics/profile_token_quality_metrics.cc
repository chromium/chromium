// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

namespace {

constexpr std::string_view kHistogramPrefix = "Autofill.ProfileTokenQuality.";

using ObservationType = ProfileTokenQuality::ObservationType;

// Emits Autofill.ProfileTokenQuality.StoredObservationsCount.PerProfile, which
// counts the total number of observations over all `types`.
void LogStoredObservationCount(const AutofillProfile& profile,
                               const ServerFieldTypeSet& types) {
  size_t total_observations = 0;
  for (ServerFieldType type : types) {
    total_observations +=
        profile.token_quality().GetObservationTypesForFieldType(type).size();
  }
  base::UmaHistogramCounts1000(
      base::StrCat({kHistogramPrefix, "StoredObservationsCount.PerProfile"}),
      total_observations);
}

// Emits Autofill.ProfileTokenQuality.StoredObservationTypes.{Type}, for every
// Type in `types`. It tracks the different observation types available for that
// Type.
void LogStoredObservationsPerType(const AutofillProfile& profile,
                                  const ServerFieldTypeSet& types) {
  for (ServerFieldType type : types) {
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
                           const ServerFieldTypeSet& types) {
  size_t total_stored_good_observations = 0, total_stored_bad_observations = 0;
  for (ServerFieldType type : types) {
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

}  // namespace

void LogStoredProfileTokenQualityMetrics(
    const std::vector<AutofillProfile*>& profiles) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillTrackProfileTokenQuality)) {
    return;
  }
  for (const AutofillProfile* profile : profiles) {
    // Observations are only stored for `stored_types`. Additional supported
    // types default to their corresponding storeable type. Since some
    // `stored_types` are only supported in profiles of certain countries,
    // intersect the two sets.
    ServerFieldTypeSet relevant_types;
    profile->GetSupportedTypes(&relevant_types);
    relevant_types.intersect(GetDatabaseStoredTypesOfAutofillProfile());

    LogStoredObservationCount(*profile, relevant_types);
    LogStoredObservationsPerType(*profile, relevant_types);
    LogStoredTokenQuality(*profile, relevant_types);
  }
}

}  // namespace autofill::autofill_metrics
