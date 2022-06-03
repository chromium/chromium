// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/trusted_vault_histograms.h"

#include "base/metrics/histogram_functions.h"

namespace syncer {

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDeviceRegistrationState",
                                registration_state);
}

// Records url fetch response status (combined http and net error code).
// Either |http_status| or |net_error| must be non zero.
void RecordTrustedVaultURLFetchResponse(int http_response_code, int net_error) {
  DCHECK_LE(net_error, 0);
  DCHECK_GE(http_response_code, 0);

  base::UmaHistogramSparse(
      "Sync.TrustedVaultURLFetchResponse",
      http_response_code == 0 ? net_error : http_response_code);
}

}  // namespace syncer
