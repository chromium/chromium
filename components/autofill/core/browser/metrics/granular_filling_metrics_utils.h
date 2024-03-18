// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_UTILS_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

// Helper methods specific for granular filling metrics.
namespace autofill {

class AutofillField;

namespace autofill_metrics {

// Given a `FillingMethod` returns its `std::string_view` representation. Also
// squashes all group filling enums to "GroupFilling".
std::string_view FillingMethodToCompactStringView(FillingMethod filling_method);

// Computes and adds the `FillingStats` of `field` to the correct key
// (`FillingMethod`) in `field_stats_by_filling_method`.
void AddFillingStatsForFillingMethod(
    const AutofillField& field,
    base::flat_map<FillingMethod, FormGroupFillingStats>&
        field_stats_by_filling_method);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_UTILS_H_
