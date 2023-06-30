// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_HISTOGRAMS_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_HISTOGRAMS_H_

#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace trusted_vault {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA {
  kRecoveryMethodAdded = 0,
  kPersistentAuthErrorResolved = 1,
  kMaxValue = kPersistentAuthErrorResolved,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TrustedVaultDeviceRegistrationStateForUMA {
  kAlreadyRegisteredV0 = 0,
  kLocalKeysAreStale = 1,
  kThrottledClientSide = 2,
  kAttemptingRegistrationWithNewKeyPair = 3,
  kAttemptingRegistrationWithExistingKeyPair = 4,
  // Deprecated, replaced with more detailed
  // TrustedVaultDeviceRegistrationOutcomeForUMA.
  kDeprecatedAttemptingRegistrationWithPersistentAuthError = 5,
  kAlreadyRegisteredV1 = 6,
  kMaxValue = kAlreadyRegisteredV1,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TrustedVaultDeviceRegistrationOutcomeForUMA {
  kSuccess = 0,
  kAlreadyRegistered = 1,
  kLocalDataObsolete = 2,
  kTransientAccessTokenFetchError = 3,
  kPersistentAccessTokenFetchError = 4,
  kPrimaryAccountChangeAccessTokenFetchError = 5,
  kNetworkError = 6,
  kOtherError = 7,
  kMaxValue = kOtherError,
};

// Used to provide UMA metric breakdowns.
enum class TrustedVaultURLFetchReasonForUMA {
  kUnspecified,
  kRegisterDevice,
  kRegisterUnspecifiedAuthenticationFactor,
  kDownloadKeys,
  kDownloadIsRecoverabilityDegraded,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TrustedVaultDownloadKeysStatusForUMA {
  kSuccess = 0,
  // Deprecated in favor of the more fine-grained buckets.
  kDeprecatedMembershipNotFoundOrCorrupted = 1,
  kNoNewKeys = 2,
  kKeyProofsVerificationFailed = 3,
  kAccessTokenFetchingFailure = 4,
  kOtherError = 5,
  kMemberNotFound = 6,
  kMembershipNotFound = 7,
  kMembershipCorrupted = 8,
  kMembershipEmpty = 9,
  kNoPrimaryAccount = 10,
  kDeviceNotRegistered = 11,
  kThrottledClientSide = 12,
  kCorruptedLocalDeviceRegistration = 13,
  kAborted = 14,
  kNetworkError = 15,
  kMaxValue = kNetworkError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TrustedVaultFileReadStatusForUMA {
  kSuccess = 0,
  kNotFound = 1,
  kFileReadFailed = 2,
  kMD5DigestMismatch = 3,
  kFileProtoDeserializationFailed = 4,
  kDataProtoDeserializationFailed = 5,
  kMaxValue = kDataProtoDeserializationFailed
};

void RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
    TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA
        hint_degraded_recoverability_changed_reason);

void RecordTrustedVaultDeviceRegistrationState(
    TrustedVaultDeviceRegistrationStateForUMA registration_state);

void RecordTrustedVaultDeviceRegistrationOutcome(
    TrustedVaultDeviceRegistrationOutcomeForUMA registration_outcome);

// Records url fetch response status (combined http and net error code). If
// |http_response_code| is non-zero, it will be recorded, otherwise |net_error|
// will be recorded. Either |http_status| or |net_error| must be non zero.
void RecordTrustedVaultURLFetchResponse(
    int http_response_code,
    int net_error,
    TrustedVaultURLFetchReasonForUMA reason);

// Records the outcome of trying to download keys from the server.
// |also_log_with_v1_suffix| allows the caller to determine whether the local
// device's registration is a V1 registration (that is, more reliable), which
// causes a second histogram to be logged as well.
void RecordTrustedVaultDownloadKeysStatus(
    TrustedVaultDownloadKeysStatusForUMA status,
    bool also_log_with_v1_suffix);

// Records the outcome of verifying a device registration status, which is
// achieved by trying to download keys (without actually having the need to
// download keys), which is the reason why the same enum is used.
// |also_log_with_v1_suffix| allows the caller to determine whether the local
// device's registration is a V1 registration (that is, more reliable), which
// causes a second histogram to be logged as well.
void RecordVerifyRegistrationStatus(TrustedVaultDownloadKeysStatusForUMA status,
                                    bool also_log_with_v1_suffix);

void RecordTrustedVaultFileReadStatus(TrustedVaultFileReadStatusForUMA status);

enum class IsOffTheRecord { kNo, kYes };

// Records a call to set security domain encryption keys in the browser.
// `absl::nullopt` indicates the caller attempted to set keys for a security
// domain with a name that was not understood by this client.
void RecordTrustedVaultSetEncryptionKeysForSecurityDomain(
    absl::optional<SecurityDomainId> security_domain,
    IsOffTheRecord is_off_the_record);

// Records a call to chrome.setClientEncryptionKeys() for the given security
// domain in the renderer. `absl::nullopt` indicates the caller attempted to set
// keys for a security domain with a name that was not understood by this
// client.
void RecordCallToJsSetClientEncryptionKeysWithSecurityDomainToUma(
    absl::optional<SecurityDomainId> security_domain);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_HISTOGRAMS_H_
