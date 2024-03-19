// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/address_save_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogManuallyAddedAddress(AutofillManuallyAddedAddressSurface surface) {
  base::UmaHistogramEnumeration("Autofill.AddedNewAddress", surface);
}

}  // namespace autofill::autofill_metrics
