// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_histograms.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace trusted_vault {

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SecurityDomainIdOrInvalidForUma {
  kInvalid = 0,
  kChromeSync = 1,
  kMaxValue = kChromeSync,
};

SecurityDomainIdOrInvalidForUma GetSecurityDomainIdOrInvalidForUma(
    absl::optional<SecurityDomainId> security_domain) {
  if (!security_domain) {
    return SecurityDomainIdOrInvalidForUma::kInvalid;
  }
  switch (*security_domain) {
    case SecurityDomainId::kChromeSync:
      return SecurityDomainIdOrInvalidForUma::kChromeSync;
  }
  NOTREACHED_NORETURN();
}

void RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
    TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA
        hint_degraded_recoverability_changed_reason) {
  // TODO(crbug.com/1423343): eventually histograms under
  // components/trusted_vault should start using their own prefix instead of
  // "Sync." and migrated to the dedicated histograms.xml file.
  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultHintDegradedRecoverabilityChangedReason2",
      hint_degraded_recoverability_changed_reason);
}

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDeviceRegistrationState",
                                registration_state);
}

void RecordTrustedVaultDeviceRegistrationOutcome(
    TrustedVaultDeviceRegistrationOutcomeForUMA registration_outcome) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDeviceRegistrationOutcome",
                                registration_outcome);
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
    TrustedVaultDownloadKeysStatusForUMA status,
    bool also_log_with_v1_suffix) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultDownloadKeysStatus", status);
  if (also_log_with_v1_suffix) {
    base::UmaHistogramEnumeration("Sync.TrustedVaultDownloadKeysStatusV1",
                                  status);
  }
}

void RecordVerifyRegistrationStatus(TrustedVaultDownloadKeysStatusForUMA status,
                                    bool also_log_with_v1_suffix) {
  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultVerifyDeviceRegistrationState", status);

  if (also_log_with_v1_suffix) {
    base::UmaHistogramEnumeration(
        "Sync.TrustedVaultVerifyDeviceRegistrationStateV1", status);
  }
}

void RecordTrustedVaultFileReadStatus(TrustedVaultFileReadStatusForUMA status) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultFileReadStatus", status);
}

void RecordTrustedVaultSetEncryptionKeysForSecurityDomain(
    absl::optional<SecurityDomainId> security_domain,
    IsOffTheRecord is_off_the_record) {
  SecurityDomainIdOrInvalidForUma domain_for_uma =
      GetSecurityDomainIdOrInvalidForUma(security_domain);
  base::UmaHistogramEnumeration(
      "TrustedVault.SetEncryptionKeysForSecurityDomain."
      "AllProfiles",
      domain_for_uma);
  if (is_off_the_record == IsOffTheRecord::kYes) {
    base::UmaHistogramEnumeration(
        "TrustedVault.SetEncryptionKeysForSecurityDomain.OffTheRecordOnly",
        domain_for_uma);
  }
}

void RecordCallToJsSetClientEncryptionKeysWithSecurityDomainToUma(
    absl::optional<SecurityDomainId> security_domain) {
  SecurityDomainIdOrInvalidForUma domain_for_uma =
      GetSecurityDomainIdOrInvalidForUma(security_domain);
  base::UmaHistogramEnumeration(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain",
      domain_for_uma);
}

}  // namespace trusted_vault
