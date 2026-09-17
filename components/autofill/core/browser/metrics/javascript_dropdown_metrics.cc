// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/javascript_dropdown_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogJavaScriptDropdownDetectionMetrics(
    JavaScriptDropdownType dropdown_type,
    base::span<const JavaScriptFieldModification> field_modifications,
    base::TimeTicks detection_start_timestamp) {
  base::UmaHistogramEnumeration("Autofill.DropdownDetection.DetectedType",
                                dropdown_type);
  if (dropdown_type == JavaScriptDropdownType::kNone) {
    return;
  }

  CHECK(!field_modifications.empty());
  base::UmaHistogramTimes(
      "Autofill.DropdownDetection.JavaScriptModificationTime.First",
      field_modifications.front().timestamp - detection_start_timestamp);
  base::UmaHistogramTimes(
      "Autofill.DropdownDetection.JavaScriptModificationTime.Last",
      field_modifications.back().timestamp - detection_start_timestamp);
  base::UmaHistogramTimes(
      "Autofill.DropdownDetection.JavaScriptModificationTime.Duration",
      field_modifications.back().timestamp -
          field_modifications.front().timestamp);
  for (const JavaScriptFieldModification& field_modification :
       field_modifications) {
    base::UmaHistogramTimes(
        "Autofill.DropdownDetection.JavaScriptModificationTime",
        field_modification.timestamp - detection_start_timestamp);
  }
}

}  // namespace autofill::autofill_metrics
