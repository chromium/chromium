// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_CONVERGE_TO_EXTREME_LENGTH_ADDRESS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_CONVERGE_TO_EXTREME_LENGTH_ADDRESS_METRICS_H_

namespace autofill::autofill_metrics {

// Records the result of trying to merge two token-equivalent addresses with
// equal verification statuses but different string-lengths, whenever the
// feature `kAutofillConvergeToExtremeLengthStreetAddress` is enabled.
void LogAddressUpdateLengthConvergenceStatus(bool convergence_status);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_CONVERGE_TO_EXTREME_LENGTH_ADDRESS_METRICS_H_
