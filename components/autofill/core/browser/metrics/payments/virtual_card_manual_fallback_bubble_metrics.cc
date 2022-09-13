// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_manual_fallback_bubble_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill::autofill_metrics {

void LogVirtualCardManualFallbackBubbleShown(bool is_reshow) {
  base::UmaHistogramBoolean("Autofill.VirtualCardManualFallbackBubble.Shown",
                            is_reshow);
}

void LogVirtualCardManualFallbackBubbleResultMetric(
    VirtualCardManualFallbackBubbleResult metric,
    bool is_reshow) {
  static const char first_show[] =
      "Autofill.VirtualCardManualFallbackBubble.Result.FirstShow";
  static const char reshows[] =
      "Autofill.VirtualCardManualFallbackBubble.Result.Reshows";
  base::UmaHistogramEnumeration(is_reshow ? reshows : first_show, metric);
}

void LogVirtualCardManualFallbackBubbleFieldClicked(
    VirtualCardManualFallbackBubbleFieldClicked metric) {
  base::UmaHistogramEnumeration(
      "Autofill.VirtualCardManualFallbackBubble.FieldClicked", metric);
}

}  // namespace autofill::autofill_metrics
