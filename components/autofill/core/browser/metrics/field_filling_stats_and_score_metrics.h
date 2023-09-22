// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_

#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

// Logs the `filling_stats` of the fields within a `form_type`. The filling
// status consistent of the number of accepted, corrected or and unfilled
// fields. See the .cc file for details.
void LogFieldFillingStatsAndScore(
    const FormGroupFillingStats& address_filling_stats,
    const FormGroupFillingStats& cc_filling_stats);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_
