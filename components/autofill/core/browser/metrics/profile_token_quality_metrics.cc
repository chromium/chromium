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
// counts the total number of observations over all supported types of the
// profile. To prevent double counting additional types, only stored supported
// types are considered.
void LogStoredObservationCount(const AutofillProfile& profile) {
  ServerFieldTypeSet supported_types;
  profile.GetSupportedTypes(&supported_types);
  size_t total_observations = 0;
  for (ServerFieldType type : GetDatabaseStoredTypesOfAutofillProfile()) {
    // Some types are only supported in profiles of certain countries.
    if (supported_types.contains(type)) {
      total_observations +=
          profile.token_quality().GetObservationTypesForFieldType(type).size();
    }
  }
  // `total_observations` can be up to ProfileTokenQuality::
  // kMaxObservationsPerToken * (number of stored and supported types).
  base::UmaHistogramCounts1000(
      base::StrCat({kHistogramPrefix, "StoredObservationsCount.PerProfile"}),
      total_observations);
}

}  // namespace

void LogStoredProfileTokenQualityMetrics(
    const std::vector<AutofillProfile*>& profiles) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillTrackProfileTokenQuality)) {
    return;
  }
  for (const AutofillProfile* profile : profiles) {
    LogStoredObservationCount(*profile);
  }
}

}  // namespace autofill::autofill_metrics
