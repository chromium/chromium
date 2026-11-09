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
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/random.h"

namespace trusted_vault {

namespace {

constexpr size_t kSharedKeyLength = 32;

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

bool ChromeSyncEnrollmentHandler::SatisfiesLocalPrerequisites() const {
  // ChromeSync enrollment is currently only possible if the device is
  // pre-enrolled (i.e. keys are not empty and there is no non-constant key).
  // Technically, it would also be possible to perform enrollment if the
  // security domain was empty / not created. However, that adds additional
  // complexity for a case that should never happen, because the trusted vault
  // client eagerly pre-enrolls into the `kChromeSync` security domain anyways.
  return !storage_
              ->GetVaultKeys(primary_account_.gaia,
                             SecurityDomainId::kChromeSync)
              .empty() &&
         !storage_->HasNonConstantKey(primary_account_.gaia,
                                      SecurityDomainId::kChromeSync);
}

void ChromeSyncEnrollmentHandler::CanEnroll(CanEnrollCallback callback) {
  if (!SatisfiesLocalPrerequisites()) {
    base::BindPostTaskToCurrentDefault(
        base::BindOnce(std::move(callback), false))
        .Run();
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
  std::vector<CanEnrollCallback> callbacks =
      std::move(ongoing_can_enroll_->callbacks);
  ongoing_can_enroll_.reset();

  const bool can_enroll =
      status == TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess &&
      SecureBoxPublicKey::CreateByImport(public_key) &&
      SatisfiesLocalPrerequisites();
  for (CanEnrollCallback& cb : callbacks) {
    std::move(cb).Run(can_enroll);
  }
}

void ChromeSyncEnrollmentHandler::Enroll(EnrollCallback callback) {
  if (!SatisfiesLocalPrerequisites() || ongoing_enroll_.has_value()) {
    base::BindPostTaskToCurrentDefault(
        base::BindOnce(std::move(callback), std::vector<uint8_t>(), -1))
        .Run();
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

  if (status != TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess ||
      !SatisfiesLocalPrerequisites()) {
    FulfillOngoingEnroll({}, -1);
    return;
  }

  std::unique_ptr<SecureBoxPublicKey> imported_public_key =
      SecureBoxPublicKey::CreateByImport(public_key);
  if (!imported_public_key) {
    FulfillOngoingEnroll({}, -1);
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
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kNetworkError:
    case TrustedVaultRegistrationStatus::kOtherError:
      FulfillOngoingEnroll({}, -1);
      return;
  }

  if (!SatisfiesLocalPrerequisites()) {
    FulfillOngoingEnroll({}, -1);
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

  if (!SatisfiesLocalPrerequisites()) {
    FulfillOngoingEnroll({}, -1);
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

  if (status != TrustedVaultKeyRotationStatus::kSuccess) {
    FulfillOngoingEnroll({}, -1);
    return;
  }

  FulfillOngoingEnroll(new_shared_key, new_key_version);
}

void ChromeSyncEnrollmentHandler::FulfillOngoingEnroll(
    const std::vector<uint8_t>& new_shared_key,
    int new_key_version) {
  CHECK(ongoing_enroll_.has_value());
  EnrollCallback cb = std::move(ongoing_enroll_->callback);
  ongoing_enroll_.reset();
  std::move(cb).Run(new_shared_key, new_key_version);
}

}  // namespace trusted_vault
