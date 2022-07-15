// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/trusted_vault_histograms.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace syncer {

namespace {

std::string GetReasonSuffix(TrustedVaultURLFetchReasonForUMA reason) {
  switch (reason) {
    case TrustedVaultURLFetchReasonForUMA::kUnspecified:
      return std::string();
    case TrustedVaultURLFetchReasonForUMA::kRegisterDevice:
      return "RegisterDevice";
    case TrustedVaultURLFetchReasonForUMA::
        kRegisterUnspecifiedAuthenticationFactor:
      return "RegisterUnspecifiedAuthenticationFactor";
    case TrustedVaultURLFetchReasonForUMA::kDownloadKeys:
      return "DownloadKeys";
    case TrustedVaultURLFetchReasonForUMA::kDownloadIsRecoverabilityDegraded:
      return "DownloadIsRecoverabilityDegraded";
  }
}

}  // namespace

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDeviceRegistrationState",
                                registration_state);
}

void RecordTrustedVaultURLFetchResponse(
    int http_response_code,
    int net_error,
    TrustedVaultURLFetchReasonForUMA reason) {
  DCHECK_LE(net_error, 0);
  DCHECK_GE(http_response_code, 0);

  const int value = http_response_code == 0 ? net_error : http_response_code;
  const std::string suffix = GetReasonSuffix(reason);

  base::UmaHistogramSparse("Sync.TrustedVaultURLFetchResponse", value);

  if (!suffix.empty()) {
    base::UmaHistogramSparse(
        base::StrCat({"Sync.TrustedVaultURLFetchResponse", ".", suffix}),
        value);
  }
}

void RecordTrustedVaultDownloadKeysStatus(
    TrustedVaultDownloadKeysStatusForUMA status) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDownloadKeysStatus", status);
}

}  // namespace syncer
