// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogSaveAndFillFormEvent(SaveAndFillFormEvent event) {
  base::UmaHistogramEnumeration("Autofill.FormEvents.CreditCard.SaveAndFill",
                                event);
}

}  // namespace autofill::autofill_metrics
