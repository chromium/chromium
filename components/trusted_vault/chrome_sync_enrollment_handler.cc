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
#include "base/task/bind_post_task.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

ChromeSyncEnrollmentHandler::OngoingCanEnroll::OngoingCanEnroll() = default;

ChromeSyncEnrollmentHandler::OngoingCanEnroll::OngoingCanEnroll(
    OngoingCanEnroll&&) noexcept = default;

ChromeSyncEnrollmentHandler::OngoingCanEnroll&
ChromeSyncEnrollmentHandler::OngoingCanEnroll::operator=(
    OngoingCanEnroll&&) noexcept = default;

ChromeSyncEnrollmentHandler::OngoingCanEnroll::~OngoingCanEnroll() = default;

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

}  // namespace trusted_vault
