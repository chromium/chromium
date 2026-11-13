// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/chrome_sync_enrollment_handler.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/random.h"

namespace trusted_vault {

namespace {

constexpr size_t kSharedKeyLength = 32;

void FulfillCanEnrollWithFailure(
    ChromeSyncEnrollmentHandler::CanEnrollCallback callback,
    TrustedVaultCanEnrollOutcomeForUMA outcome) {
  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(callback), false, outcome))
      .Run();
}

void FulfillEnrollWithFailure(
    ChromeSyncEnrollmentHandler::EnrollCallback callback,
    TrustedVaultEnrollOutcomeForUMA outcome) {
  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(callback), std::vector<uint8_t>(), -1, outcome))
      .Run();
}

}  // namespace

ChromeSyncEnrollmentHandler::OngoingCanEnroll::OngoingCanEnroll() = default;

ChromeSyncEnrollmentHandler::OngoingCanEnroll::OngoingCanEnroll(
    OngoingCanEnroll&&) noexcept = default;

ChromeSyncEnrollmentHandler::OngoingCanEnroll&
ChromeSyncEnrollmentHandler::OngoingCanEnroll::operator=(
    OngoingCanEnroll&&) noexcept = default;

ChromeSyncEnrollmentHandler::OngoingCanEnroll::~OngoingCanEnroll() = default;

ChromeSyncEnrollmentHandler::OngoingEnroll::OngoingEnroll() = default;

ChromeSyncEnrollmentHandler::OngoingEnroll::OngoingEnroll(
    OngoingEnroll&&) noexcept = default;

ChromeSyncEnrollmentHandler::OngoingEnroll&
ChromeSyncEnrollmentHandler::OngoingEnroll::operator=(
    OngoingEnroll&&) noexcept = default;

ChromeSyncEnrollmentHandler::OngoingEnroll::~OngoingEnroll() = default;

ChromeSyncEnrollmentHandler::ChromeSyncEnrollmentHandler(
    CoreAccountInfo primary_account,
    KeyStorage* storage,
    TrustedVaultConnection* connection)
    : primary_account_(std::move(primary_account)),
      storage_(CHECK_DEREF(storage)),
      connection_(CHECK_DEREF(connection)) {}

ChromeSyncEnrollmentHandler::~ChromeSyncEnrollmentHandler() = default;

// static
TrustedVaultCanEnrollOutcomeForUMA
ChromeSyncEnrollmentHandler::GetCanEnrollOutcomeForUMAFromLocalIneligibility(
    LocalIneligibility local_ineligibility) {
  switch (local_ineligibility) {
    case LocalIneligibility::kAlreadyEnrolled:
      return TrustedVaultCanEnrollOutcomeForUMA::kAlreadyEnrolled;
    case LocalIneligibility::kNoLocalKeys:
      return TrustedVaultCanEnrollOutcomeForUMA::kNoLocalKeys;
  }
  NOTREACHED();
}

// static
TrustedVaultEnrollOutcomeForUMA
ChromeSyncEnrollmentHandler::GetEnrollOutcomeForUMAFromLocalIneligibility(
    LocalIneligibility local_ineligibility) {
  switch (local_ineligibility) {
    case LocalIneligibility::kAlreadyEnrolled:
      return TrustedVaultEnrollOutcomeForUMA::kAlreadyEnrolled;
    case LocalIneligibility::kNoLocalKeys:
      return TrustedVaultEnrollOutcomeForUMA::kNoLocalKeys;
  }
  NOTREACHED();
}

std::optional<ChromeSyncEnrollmentHandler::LocalIneligibility>
ChromeSyncEnrollmentHandler::GetLocalIneligibility() const {
  // ChromeSync enrollment is currently only possible if the security domain is
  // in the pre-enrollment state according to local storage (i.e. keys are not
  // empty and there is no non-constant key). Note that this does not check
  // whether the local device itself is registered: enrollment still works if
  // the constant key was stored locally by a client; storing the rotated key
  // locally afterwards allows the device to register.
  // Technically, it would also be possible to perform enrollment if the
  // security domain was empty / not created. However, that adds additional
  // complexity for a case that should never happen, because the trusted vault
  // client eagerly pre-enrolls into the `kChromeSync` security domain anyways.
  if (storage_->HasNonConstantKey(primary_account_.gaia,
                                  SecurityDomainId::kChromeSync)) {
    return LocalIneligibility::kAlreadyEnrolled;
  }
  if (storage_
          ->GetVaultKeys(primary_account_.gaia, SecurityDomainId::kChromeSync)
          .empty()) {
    return LocalIneligibility::kNoLocalKeys;
  }
  return std::nullopt;
}

void ChromeSyncEnrollmentHandler::CanEnroll(CanEnrollCallback callback) {
  if (const std::optional<LocalIneligibility> local_ineligibility =
          GetLocalIneligibility()) {
    FulfillCanEnrollWithFailure(
        std::move(callback),
        GetCanEnrollOutcomeForUMAFromLocalIneligibility(*local_ineligibility));
    return;
  }

  if (ongoing_can_enroll_.has_value()) {
    ongoing_can_enroll_->callbacks.push_back(std::move(callback));
    return;
  }
  ongoing_can_enroll_.emplace();
  ongoing_can_enroll_->callbacks.push_back(std::move(callback));
  // `this` outlives `ongoing_can_enroll_`.
  ongoing_can_enroll_->request = connection_->DownloadGaiaPasswordPublicKey(
      primary_account_,
      base::BindOnce(&ChromeSyncEnrollmentHandler::
                         OnGaiaPasswordPublicKeyDownloadedForCanEnroll,
                     base::Unretained(this)));
}

void ChromeSyncEnrollmentHandler::OnGaiaPasswordPublicKeyDownloadedForCanEnroll(
    TrustedVaultDownloadPasswordPublicKeyStatus status,
    const std::vector<uint8_t>& public_key) {
  CHECK(ongoing_can_enroll_.has_value());
  ongoing_can_enroll_->request.reset();

  if (const std::optional<LocalIneligibility> local_ineligibility =
          GetLocalIneligibility()) {
    FulfillOngoingCanEnrollWithFailure(
        GetCanEnrollOutcomeForUMAFromLocalIneligibility(*local_ineligibility));
    return;
  }

  switch (status) {
    case TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess:
      break;
    case TrustedVaultDownloadPasswordPublicKeyStatus::kNoPublicKey:
      FulfillOngoingCanEnrollWithFailure(
          TrustedVaultCanEnrollOutcomeForUMA::kNoEligibleGaiaPassword);
      return;
    case TrustedVaultDownloadPasswordPublicKeyStatus::
        kTransientAccessTokenFetchError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::
        kPersistentAccessTokenFetchError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::kNetworkError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::kOtherError:
      FulfillOngoingCanEnrollWithFailure(
          TrustedVaultCanEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
      return;
  }

  if (!SecureBoxPublicKey::CreateByImport(public_key)) {
    FulfillOngoingCanEnrollWithFailure(
        TrustedVaultCanEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
    return;
  }

  std::vector<CanEnrollCallback> callbacks =
      std::move(ongoing_can_enroll_->callbacks);
  ongoing_can_enroll_.reset();
  for (CanEnrollCallback& cb : callbacks) {
    std::move(cb).Run(true, TrustedVaultCanEnrollOutcomeForUMA::kEligible);
  }
}

void ChromeSyncEnrollmentHandler::FulfillOngoingCanEnrollWithFailure(
    TrustedVaultCanEnrollOutcomeForUMA outcome) {
  CHECK(ongoing_can_enroll_.has_value());
  std::vector<CanEnrollCallback> callbacks =
      std::move(ongoing_can_enroll_->callbacks);
  ongoing_can_enroll_.reset();
  for (CanEnrollCallback& cb : callbacks) {
    FulfillCanEnrollWithFailure(std::move(cb), outcome);
  }
}

void ChromeSyncEnrollmentHandler::Enroll(EnrollCallback callback) {
  if (const std::optional<LocalIneligibility> local_ineligibility =
          GetLocalIneligibility()) {
    FulfillEnrollWithFailure(
        std::move(callback),
        GetEnrollOutcomeForUMAFromLocalIneligibility(*local_ineligibility));
    return;
  }

  if (ongoing_enroll_.has_value()) {
    FulfillEnrollWithFailure(
        std::move(callback),
        TrustedVaultEnrollOutcomeForUMA::kAlreadyInProgress);
    return;
  }

  ongoing_enroll_.emplace();
  ongoing_enroll_->callback = std::move(callback);
  // `this` outlives `ongoing_enroll_`.
  ongoing_enroll_->request = connection_->DownloadGaiaPasswordPublicKey(
      primary_account_,
      base::BindOnce(&ChromeSyncEnrollmentHandler::
                         OnGaiaPasswordPublicKeyDownloadedForEnroll,
                     base::Unretained(this)));
}

void ChromeSyncEnrollmentHandler::OnGaiaPasswordPublicKeyDownloadedForEnroll(
    TrustedVaultDownloadPasswordPublicKeyStatus status,
    const std::vector<uint8_t>& public_key) {
  CHECK(ongoing_enroll_.has_value());
  ongoing_enroll_->request.reset();

  if (const std::optional<LocalIneligibility> local_ineligibility =
          GetLocalIneligibility()) {
    FulfillOngoingEnrollWithFailure(
        GetEnrollOutcomeForUMAFromLocalIneligibility(*local_ineligibility));
    return;
  }

  switch (status) {
    case TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess:
      break;
    case TrustedVaultDownloadPasswordPublicKeyStatus::kNoPublicKey:
      FulfillOngoingEnrollWithFailure(
          TrustedVaultEnrollOutcomeForUMA::kNoEligibleGaiaPassword);
      return;
    case TrustedVaultDownloadPasswordPublicKeyStatus::
        kTransientAccessTokenFetchError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::
        kPersistentAccessTokenFetchError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::kNetworkError:
    case TrustedVaultDownloadPasswordPublicKeyStatus::kOtherError:
      FulfillOngoingEnrollWithFailure(
          TrustedVaultEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
      return;
  }

  std::unique_ptr<SecureBoxPublicKey> imported_public_key =
      SecureBoxPublicKey::CreateByImport(public_key);
  if (!imported_public_key) {
    FulfillOngoingEnrollWithFailure(
        TrustedVaultEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
    return;
  }

  const std::vector<std::vector<uint8_t>> vault_keys = storage_->GetVaultKeys(
      primary_account_.gaia, SecurityDomainId::kChromeSync);
  CHECK(!vault_keys.empty());
  const int last_key_version = storage_->GetLastKeyVersion(
      primary_account_.gaia, SecurityDomainId::kChromeSync);

  // `this` outlives `ongoing_enroll_`.
  ongoing_enroll_->request = connection_->RegisterAuthenticationFactor(
      primary_account_, SecurityDomainId::kChromeSync,
      GetTrustedVaultKeysWithVersions(vault_keys, last_key_version),
      *imported_public_key, GaiaPassword(),
      base::BindOnce(
          &ChromeSyncEnrollmentHandler::OnGaiaPasswordRegisteredForEnroll,
          base::Unretained(this)));
}

void ChromeSyncEnrollmentHandler::OnGaiaPasswordRegisteredForEnroll(
    TrustedVaultRegistrationStatus status,
    int unused_epoch) {
  CHECK(ongoing_enroll_.has_value());
  ongoing_enroll_->request.reset();

  switch (status) {
    case TrustedVaultRegistrationStatus::kRegistrationNotAttempted:
    case TrustedVaultRegistrationStatus::kRegistrationCancelled:
      // Those states are never reported by
      // `TrustedVaultConnection::RegisterAuthenticationFactor`.
      // TODO(crbug.com/583780874): Split `LocalRecoveryFactor`-only states out
      // of `TrustedVaultRegistrationStatus` to avoid unreachable branches.
      NOTREACHED();
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      break;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      FulfillOngoingEnrollWithFailure(
          TrustedVaultEnrollOutcomeForUMA::kLocalDataObsolete);
      return;
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kNetworkError:
    case TrustedVaultRegistrationStatus::kOtherError:
      FulfillOngoingEnrollWithFailure(
          TrustedVaultEnrollOutcomeForUMA::kGaiaPasswordRegistrationFailed);
      return;
  }

  if (const std::optional<LocalIneligibility> local_ineligibility =
          GetLocalIneligibility()) {
    FulfillOngoingEnrollWithFailure(
        GetEnrollOutcomeForUMAFromLocalIneligibility(*local_ineligibility));
    return;
  }

  const std::vector<uint8_t> new_shared_key =
      crypto::RandBytesAsVector(kSharedKeyLength);
  RotateSharedKeyForEnroll(new_shared_key);
}

void ChromeSyncEnrollmentHandler::RotateSharedKeyForEnroll(
    const std::vector<uint8_t>& new_shared_key) {
  CHECK(ongoing_enroll_.has_value());
  const std::vector<std::vector<uint8_t>> vault_keys = storage_->GetVaultKeys(
      primary_account_.gaia, SecurityDomainId::kChromeSync);
  CHECK(!vault_keys.empty());
  const int last_key_version = storage_->GetLastKeyVersion(
      primary_account_.gaia, SecurityDomainId::kChromeSync);
  // `this` outlives `ongoing_enroll_`.
  ongoing_enroll_->request = connection_->RotateSharedKey(
      primary_account_, SecurityDomainId::kChromeSync,
      TrustedVaultKeyAndVersion(vault_keys.back(), last_key_version),
      new_shared_key,
      base::BindOnce(&ChromeSyncEnrollmentHandler::OnKeyRotatedForEnroll,
                     base::Unretained(this), new_shared_key));
}

void ChromeSyncEnrollmentHandler::OnKeyRotatedForEnroll(
    const std::vector<uint8_t>& new_shared_key,
    TrustedVaultKeyRotationStatus status,
    int new_key_version) {
  CHECK(ongoing_enroll_.has_value());
  ongoing_enroll_->request.reset();

  if (const std::optional<LocalIneligibility> local_ineligibility =
          GetLocalIneligibility()) {
    FulfillOngoingEnrollWithFailure(
        GetEnrollOutcomeForUMAFromLocalIneligibility(*local_ineligibility));
    return;
  }

  // Adding a new recovery factor right before key rotation can trigger
  // asynchronous member cleanup on the backend, which may race with
  // listing security domain members in RotateSharedKey and cause a
  // membership mismatch (HTTP 400). Retry the rotation once if this happens.
  if (status == TrustedVaultKeyRotationStatus::kMembershipMismatch &&
      !ongoing_enroll_->has_retried_key_rotation) {
    ongoing_enroll_->has_retried_key_rotation = true;
    RotateSharedKeyForEnroll(new_shared_key);
    return;
  }

  switch (status) {
    case TrustedVaultKeyRotationStatus::kSuccess: {
      const TrustedVaultEnrollOutcomeForUMA outcome =
          ongoing_enroll_->has_retried_key_rotation
              ? TrustedVaultEnrollOutcomeForUMA::kSuccessAfterRetry
              : TrustedVaultEnrollOutcomeForUMA::kSuccess;
      EnrollCallback cb = std::move(ongoing_enroll_->callback);
      ongoing_enroll_.reset();
      std::move(cb).Run(new_shared_key, new_key_version, outcome);
      return;
    }
    case TrustedVaultKeyRotationStatus::kLocalDataObsolete:
      FulfillOngoingEnrollWithFailure(
          TrustedVaultEnrollOutcomeForUMA::kLocalDataObsolete);
      return;
    case TrustedVaultKeyRotationStatus::kMembershipMismatch:
    case TrustedVaultKeyRotationStatus::kEmptySecurityDomain:
    case TrustedVaultKeyRotationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultKeyRotationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultKeyRotationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultKeyRotationStatus::kNetworkError:
    case TrustedVaultKeyRotationStatus::kKeyProofsVerificationFailed:
    case TrustedVaultKeyRotationStatus::kOtherError:
      FulfillOngoingEnrollWithFailure(
          TrustedVaultEnrollOutcomeForUMA::kKeyRotationFailed);
      return;
  }
}

void ChromeSyncEnrollmentHandler::FulfillOngoingEnrollWithFailure(
    TrustedVaultEnrollOutcomeForUMA outcome) {
  CHECK(ongoing_enroll_.has_value());
  EnrollCallback cb = std::move(ongoing_enroll_->callback);
  ongoing_enroll_.reset();
  FulfillEnrollWithFailure(std::move(cb), outcome);
}

}  // namespace trusted_vault
