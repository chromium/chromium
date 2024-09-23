// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_standalone_cvc_suggestion_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

void LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
    VirtualCardStandaloneCvcSuggestionFormEvent event) {
  // TODO: crbug.com/362988980 - Reuse Autofill.FormEvents.StandaloneCvc after
  // launch of virtual card card-on-file project.
  base::UmaHistogramEnumeration("Autofill.VirtualCard.StandaloneCvc.FormEvents",
                                event);
}

}  // namespace autofill::autofill_metrics
