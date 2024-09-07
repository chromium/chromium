// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_STORED_PROFILE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_STORED_PROFILE_METRICS_H_

#include <stddef.h>

#include <string_view>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

// The data logged for the stored profiles metric. This is counted separately
// for each `AutofillProfileCategory`.
struct StoredProfileCounts {
  // Total number of stored profiles of the corresponding category.
  size_t total = 0;
  // The subset of profiles that hasn't been used in a fixed period of time.
  size_t disused = 0;
};

// Records statistics for the number of used, disused, and potentially,
// depending on the `category`, country-less address profiles.
// This metric is emitted each time a new Chrome profile is started. It is
// tracked separately for each `category`.
void LogStoredProfileCountStatistics(AutofillProfileRecordTypeCategory category,
                                     const StoredProfileCounts& counts);

// Records the number of days since an address profile was last used. This is
// logged separately for each profile of every `category`, each time a new
// Chrome profile is launched.
void LogStoredProfileDaysSinceLastUse(
    AutofillProfileRecordTypeCategory category,
    size_t days);

// Logs the `LogStoredProfileCountStatistics()` and
// `LogStoredProfileDaysSinceLastUse()` metrics for every
// AutofillProfileRecordTypeCategory and the corresponding subset of `profiles`.
void LogStoredProfileMetrics(
    const std::vector<const AutofillProfile*>& profiles);

// Logs the number of `kLocalOrSynable` profiles that are a strict superset of
// some `kAccount` profile. This corresponds to the number of profiles that
// cannot be automatically deduplicated, since no profiles should be silently
// deleted from the account storage.
// Comparisons are done by the `app_locale`-based `AutofillProfileComparator`.
void LogLocalProfileSupersetMetrics(
    std::vector<const AutofillProfile*> profiles,
    std::string_view app_locale);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_STORED_PROFILE_METRICS_H_
