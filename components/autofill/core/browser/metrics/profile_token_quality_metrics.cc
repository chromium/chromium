// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"

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
    for (ProfileTokenQuality::ObservationType observation :
         profile.token_quality().GetObservationTypesForFieldType(type)) {
      base::UmaHistogramEnumeration(
          base::StrCat({kHistogramPrefix, "StoredObservationTypes.",
                        FieldTypeToStringView(type)}),
          observation);
    }
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
  }
}

}  // namespace autofill::autofill_metrics
