// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_TOKEN_QUALITY_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_TOKEN_QUALITY_METRICS_H_

#include <vector>

namespace autofill {

class AutofillProfile;
class AddressDataManager;
class FormStructure;

namespace autofill_metrics {

// Emits metrics for the `ProfileTokenQuality` of every provided `profiles`.
// See implementation for an overview of all the metrics.
void LogStoredProfileTokenQualityMetrics(
    const std::vector<const AutofillProfile*>& profiles);

// Records the {number of observations (bits 0-3, capped at 10), quality score
// (bits 4-7), profile token (bits 8-15)} as a bitmask, if there were any
// observations. The score is an integer ranging from 0 to 10, as is the number
// of observations. Observations are from the profile that was used for filling.
// Emitted on form submission, after the profile's observations were updated.
void LogProfileTokenQualityScoreMetric(const FormStructure& form,
                                       const AddressDataManager& adm);

}  // namespace autofill_metrics

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_TOKEN_QUALITY_METRICS_H_
