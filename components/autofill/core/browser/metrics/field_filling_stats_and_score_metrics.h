// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

enum class FieldFillingStat {
  kAccepted = 0,
  kCorrected = 1,
  kManuallyFilled = 2,
  kLeftEmpty = 3,
  kMaxValue = kLeftEmpty
};

// Logs the `filling_stats` of the fields within a `form_type`, and the
// `filling_stats` of ac=unrecognized fields. The filling status
// consistent of the number of accepted, corrected or and unfilled fields. See
// the .cc file for details.
void LogFieldFillingStatsAndScore(
    const FormGroupFillingStats& address_filling_stats,
    const FormGroupFillingStats& cc_filling_stats,
    const FormGroupFillingStats& ac_unrecognized_address_field_stats);

// Same as above but keyed by `FillingMethod`.
void LogAddressFieldFillingStatsAndScoreByFillingMethod(
    const base::flat_map<FillingMethod,
                         autofill_metrics::FormGroupFillingStats>&
        address_filling_stats_by_filling_method);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_
