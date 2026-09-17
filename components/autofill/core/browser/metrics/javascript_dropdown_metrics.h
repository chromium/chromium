// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_JAVASCRIPT_DROPDOWN_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_JAVASCRIPT_DROPDOWN_METRICS_H_

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill::autofill_metrics {

// Logs metrics related to JS dropdown detection. Assumes `field_modifications`
// is sorted by `JavaScriptFieldModification::timestamp`.
void LogJavaScriptDropdownDetectionMetrics(
    JavaScriptDropdownType dropdown_type,
    base::span<const JavaScriptFieldModification> field_modifications,
    base::TimeTicks detection_start_timestamp);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_JAVASCRIPT_DROPDOWN_METRICS_H_
