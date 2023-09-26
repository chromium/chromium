// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace plus_addresses {

void PlusAddressMetrics::RecordModalEvent(
    PlusAddressModalEvent plus_address_modal_event) {
  base::UmaHistogramEnumeration("Autofill.PlusAddresses.Modal.Events",
                                plus_address_modal_event);
}
}  // namespace plus_addresses
