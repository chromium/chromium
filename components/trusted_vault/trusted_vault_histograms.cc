// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_histograms.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

namespace {

std::string GetTrustedVaultURLFetchReasonSuffix(
    TrustedVaultURLFetchReasonForUMA reason) {
  switch (reason) {
    case TrustedVaultURLFetchReasonForUMA::kUnspecified:
      return std::string();
    case TrustedVaultURLFetchReasonForUMA::kRegisterDevice:
      return "RegisterDevice";
    case TrustedVaultURLFetchReasonForUMA::kRegisterLockScreenKnowledgeFactor:
      return "RegisterLockScreenKnowledgeFactor";
    case TrustedVaultURLFetchReasonForUMA::kRegisterGpmPin:
      return "RegisterGooglePasswordManagerPIN";
    case TrustedVaultURLFetchReasonForUMA::
        kRegisterUnspecifiedAuthenticationFactor:
      return "RegisterUnspecifiedAuthenticationFactor";
    case TrustedVaultURLFetchReasonForUMA::kDownloadKeys:
      return "DownloadKeys";
    case TrustedVaultURLFetchReasonForUMA::kDownloadIsRecoverabilityDegraded:
      return "DownloadIsRecoverabilityDegraded";
    case TrustedVaultURLFetchReasonForUMA::
        kDownloadAuthenticationFactorsRegistrationState:
      return "DownloadAuthenticationFactorsRegistrationState";
    case TrustedVaultURLFetchReasonForUMA::kRegisterICloudKeychain:
      return "RegisterICloudKeychain";
  }

  NOTREACHED();
}

std::string GetRecoveryKeyStoreURLFetchReasonSuffix(
    RecoveryKeyStoreURLFetchReasonForUMA reason) {
  switch (reason) {
    case RecoveryKeyStoreURLFetchReasonForUMA::kUpdateRecoveryKeyStore:
      return "UpdateRecoveryKeyStore";
    case RecoveryKeyStoreURLFetchReasonForUMA::kListRecoveryKeyStores:
      return "ListRecoveryKeyStores";
  }

  NOTREACHED();
}

}  // namespace

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SecurityDomainIdOrInvalidForUma {
  kInvalid = 0,
  kChromeSync = 1,
  kPasskeys = 2,
  kMaxValue = kPasskeys,
};

SecurityDomainIdOrInvalidForUma GetSecurityDomainIdOrInvalidForUma(
    std::optional<SecurityDomainId> security_domain) {
  if (!security_domain) {
    return SecurityDomainIdOrInvalidForUma::kInvalid;
  }
  switch (*security_domain) {
    case SecurityDomainId::kChromeSync:
      return SecurityDomainIdOrInvalidForUma::kChromeSync;
    case SecurityDomainId::kPasskeys:
      return SecurityDomainIdOrInvalidForUma::kPasskeys;
  }
  NOTREACHED();
}

void RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
    TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA
        hint_degraded_recoverability_changed_reason) {
  base::UmaHistogramEnumeration(
      "TrustedVault.TrustedVaultHintDegradedRecoverabilityChangedReason",
      hint_degraded_recoverability_changed_reason);
}

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state) {
  RecordTrustedVaultRecoveryFactorRegistrationState(
      LocalRecoveryFactorType::kPhysicalDevice, SecurityDomainId::kChromeSync,
      registration_state);
}

void RecordTrustedVaultRecoveryFactorRegistrationState(
    LocalRecoveryFactorType local_recovery_factor_type,
    SecurityDomainId security_domain_id,
    TrustedVaultRecoveryFactorRegistrationStateForUMA registration_state) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"TrustedVault.RecoveryFactorRegistrationState.",
           GetLocalRecoveryFactorNameForUma(local_recovery_factor_type), ".",
           GetSecurityDomainNameForUma(security_domain_id)}),
      registration_state);
}

void RecordTrustedVaultRecoveryFactorRegistrationOutcome(
    LocalRecoveryFactorType local_recovery_factor_type,
    SecurityDomainId security_domain_id,
    TrustedVaultRecoveryFactorRegistrationOutcomeForUMA registration_outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"TrustedVault.RecoveryFactorRegistrationOutcome.",
           GetLocalRecoveryFactorNameForUma(local_recovery_factor_type), ".",
           GetSecurityDomainNameForUma(security_domain_id)}),
      registration_outcome);
}

void RecordTrustedVaultURLFetchResponse(SecurityDomainId security_domain_id,
                                        TrustedVaultURLFetchReasonForUMA reason,
                                        int http_response_code,
                                        int net_error) {
  CHECK_LE(net_error, 0);
  CHECK_GE(http_response_code, 0);

  const int value = http_response_code == 0 ? net_error : http_response_code;
  const std::string reason_suffix = GetTrustedVaultURLFetchReasonSuffix(reason);
  const std::string security_domain_name =
      GetSecurityDomainNameForUma(security_domain_id);

  base::UmaHistogramSparse("TrustedVault.SecurityDomainServiceURLFetchResponse",
                           value);
  base::UmaHistogramSparse(
      base::StrCat({"TrustedVault.SecurityDomainServiceURLFetchResponse", ".",
                    security_domain_name}),
      value);

  if (!reason_suffix.empty()) {
    base::UmaHistogramSparse(
        base::StrCat({"TrustedVault.SecurityDomainServiceURLFetchResponse", ".",
                      reason_suffix}),
        value);
    base::UmaHistogramSparse(
        base::StrCat({"TrustedVault.SecurityDomainServiceURLFetchResponse", ".",
                      reason_suffix, ".", security_domain_name}),
        value);
  }
}

void RecordRecoveryKeyStoreURLFetchResponse(
    RecoveryKeyStoreURLFetchReasonForUMA reason,
    int http_response_code,
    int net_error) {
  CHECK_LE(net_error, 0);
  CHECK_GE(http_response_code, 0);

  const int value = http_response_code == 0 ? net_error : http_response_code;
  const std::string reason_suffix =
      GetRecoveryKeyStoreURLFetchReasonSuffix(reason);

  base::UmaHistogramSparse("TrustedVault.RecoveryKeyStoreURLFetchResponse",
                           value);
  base::UmaHistogramSparse(
      base::StrCat({"TrustedVault.RecoveryKeyStoreURLFetchResponse", ".",
                    reason_suffix}),
      value);
}

void RecordTrustedVaultDownloadKeysStatus(
    LocalRecoveryFactorType local_recovery_factor_type,
    SecurityDomainId security_domain_id,
    TrustedVaultDownloadKeysStatusForUMA status) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"TrustedVault.DownloadKeysStatus.",
           GetLocalRecoveryFactorNameForUma(local_recovery_factor_type), ".",
           GetSecurityDomainNameForUma(security_domain_id)}),
      status);
}

void RecordTrustedVaultDownloadKeysStatus(
    TrustedVaultDownloadKeysStatusForUMA status) {
  RecordTrustedVaultDownloadKeysStatus(LocalRecoveryFactorType::kPhysicalDevice,
                                       SecurityDomainId::kChromeSync, status);
}

void RecordTrustedVaultRecoverKeysOutcome(
    SecurityDomainId security_domain_id,
    TrustedVaultRecoverKeysOutcomeForUMA status) {
  base::UmaHistogramEnumeration(
      base::StrCat({"TrustedVault.RecoverKeysOutcome.",
                    GetSecurityDomainNameForUma(security_domain_id)}),
      status);
}

void RecordTrustedVaultFileReadStatus(SecurityDomainId security_domain_id,
                                      TrustedVaultFileReadStatusForUMA status) {
  base::UmaHistogramEnumeration(
      "TrustedVault.FileReadStatus." +
          GetSecurityDomainNameForUma(security_domain_id),
      status);
}

void RecordTrustedVaultSetEncryptionKeysForSecurityDomain(
    std::optional<SecurityDomainId> security_domain,
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
    std::optional<SecurityDomainId> security_domain) {
  SecurityDomainIdOrInvalidForUma domain_for_uma =
      GetSecurityDomainIdOrInvalidForUma(security_domain);
  base::UmaHistogramEnumeration(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain",
      domain_for_uma);
}

void RecordTrustedVaultListSecurityDomainMembersPinStatus(
    SecurityDomainId security_domain_id,
    TrustedVaultListSecurityDomainMembersPinStatus status) {
  base::UmaHistogramEnumeration(
      "TrustedVault.ListSecurityDomainMembersPinStatus." +
          GetSecurityDomainNameForUma(security_domain_id),
      status);
}

std::string GetLocalRecoveryFactorNameForUma(
    LocalRecoveryFactorType local_recovery_factor_type) {
  // These strings get embedded in histogram names and so should not be
  // changed.
  switch (local_recovery_factor_type) {
    case LocalRecoveryFactorType::kPhysicalDevice:
      return "PhysicalDevice";
#if BUILDFLAG(IS_MAC)
    case LocalRecoveryFactorType::kICloudKeychain:
      return "ICloudKeychain";
#endif
      // If adding a new value, also update the variants for
      // LocalRecoveryFactorType in
      // tools/metrics/histograms/metadata/trusted_vault/histograms.xml.
  }
}

std::string GetSecurityDomainNameForUma(SecurityDomainId domain) {
  switch (domain) {
    // These strings get embedded in histogram names and so should not be
    // changed.
    case SecurityDomainId::kChromeSync:
      return "ChromeSync";
    case SecurityDomainId::kPasskeys:
      return "HwProtected";
      // If adding a new value, also update the variants for SecurityDomainId
      // in tools/metrics/histograms/metadata/trusted_vault/histograms.xml.
  }
}

}  // namespace trusted_vault
