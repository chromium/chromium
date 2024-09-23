// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/cryptohome_fingerprint_engine.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "cryptohome_based_engine.h"
#include "cryptohome_fingerprint_engine.h"

namespace ash {

CryptohomeFingerprintEngine::CryptohomeFingerprintEngine(
    CryptohomeCore& core,
    PrefService* local_state)
    : CryptohomeBasedEngine(core, AshAuthFactor::kFingerprint),
      local_state_(local_state) {}

CryptohomeFingerprintEngine::~CryptohomeFingerprintEngine() = default;

std::optional<cryptohome::AuthFactorRef>
CryptohomeFingerprintEngine::LookUpFactor(UserContext& context) {
  const cryptohome::AuthFactor* fingerprint_factor =
      context.GetAuthFactorsData().FindFactorByType(
          cryptohome::AuthFactorType::kFingerprint);
  if (!fingerprint_factor) {
    return std::nullopt;
  }
  return fingerprint_factor->ref();
}

// TODO(b/286526858): Policy disabled check.
bool CryptohomeFingerprintEngine::IsDisabledByPolicy() {
  return false;
}

// TODO(b/286526858): Lock out check.
bool CryptohomeFingerprintEngine::IsLockedOut() {
  return false;
}

// TODO(b/286526858): Factor restriction check.
bool CryptohomeFingerprintEngine::IsFactorSpecificRestricted() {
  return false;
}

void CryptohomeFingerprintEngine::SetUsageAllowed(UsageAllowed usage) {
  CryptohomeBasedEngine::SetUsageAllowed(usage);
  if (usage == UsageAllowed::kEnabled && stage_ == Stage::kPrepared) {
    // Immediately trigger the cached fingerprint attempt in parallel.
    if (buffered_attempt_) {
      buffered_attempt_ = false;
      PerformFingerprintAttempt();
      return;
    }
  }
  // Run the required preparation for fingerprint auth factor once
  // the engine is enabled and not prepared.
  if (usage == UsageAllowed::kEnabled && stage_ == Stage::kIdle) {
    get_core()->BorrowContext(
        base::BindOnce(&CryptohomeFingerprintEngine::PerformPreparation,
                       weak_factory_.GetWeakPtr()));
    return;
  }
}

void CryptohomeFingerprintEngine::CleanUp(CleanupCallback callback) {
  switch (stage_) {
    case Stage::kIdle:
    case Stage::kTerminating:
      break;
    case Stage::kPreparing:
    case Stage::kPrepared:
      get_core()->BorrowContext(
          base::BindOnce(&CryptohomeFingerprintEngine::PerformTermination,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
  }
  std::move(callback).Run(GetFactor());
}

void CryptohomeFingerprintEngine::PerformFingerprintAttempt() {
  if (get_usage_allowed() == UsageAllowed::kDisabledParallelAttempt) {
    // Remember the attempt and trigger it when the usage is changed to enabled.
    buffered_attempt_ = true;
    return;
  }
  if (get_usage_allowed() == UsageAllowed::kDisabled) {
    // Ignore the attempt.
    return;
  }

  CHECK(get_ref().has_value());
  get_observer()->OnFactorAttempt(GetFactor());
  get_core()->BorrowContext(
      base::BindOnce(&CryptohomeFingerprintEngine::PerformAuthentication,
                     weak_factory_.GetWeakPtr()));
}

// TODO(b/286526858): auth factor update.
void CryptohomeFingerprintEngine::OnAuthFactorUpdate(
    cryptohome::AuthFactorRef factor) {}

void CryptohomeFingerprintEngine::OnFingerprintAuthScan(
    const ::user_data_auth::AuthScanDone& result) {
  // Implementation detail: |result| currently contains success only, indicating
  // a fp touch event. The actual scan/match result will be retrieved by
  // AuthPerformer.
  PerformFingerprintAttempt();
}

void CryptohomeFingerprintEngine::PerformPreparation(
    std::unique_ptr<UserContext> context) {
  CHECK_EQ(stage_, Stage::kIdle);
  stage_ = Stage::kPreparing;
  get_core()->GetAuthPerformer()->PrepareAuthFactor(
      std::move(context), cryptohome::AuthFactorType::kFingerprint,
      base::BindOnce(&CryptohomeFingerprintEngine::OnPrepareAuthFactor,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomeFingerprintEngine::PerformAuthentication(
    std::unique_ptr<UserContext> context) {
  CHECK_EQ(stage_, Stage::kPrepared);
  get_core()->GetAuthPerformer()->AuthenticateWithFingerprint(
      std::move(context),
      base::BindOnce(&CryptohomeFingerprintEngine::OnAuthAttempt,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomeFingerprintEngine::PerformTermination(
    CleanupCallback callback,
    std::unique_ptr<UserContext> context) {
  CHECK_EQ(stage_, Stage::kPrepared);
  stage_ = Stage::kTerminating;
  get_core()->GetAuthPerformer()->TerminateAuthFactor(
      std::move(context), cryptohome::AuthFactorType::kFingerprint,
      base::BindOnce(&CryptohomeFingerprintEngine::OnTerminateAuthFactor,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CryptohomeFingerprintEngine::OnAuthAttempt(
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  get_core()->ReturnContext(std::move(context));
  get_observer()->OnFactorAttemptResult(GetFactor(),
                                        /* success= */ !error.has_value());
}

void CryptohomeFingerprintEngine::OnPrepareAuthFactor(
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  CHECK_EQ(stage_, Stage::kPreparing);
  get_core()->ReturnContext(std::move(context));
  if (error.has_value()) {
    LOG(ERROR)
        << "Ignoring fp factor as a fp sensor session fails to initiate.";
    return;
  }
  stage_ = Stage::kPrepared;
}

void CryptohomeFingerprintEngine::OnTerminateAuthFactor(
    CleanupCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  CHECK_EQ(stage_, Stage::kTerminating);
  get_core()->ReturnContext(std::move(context));

  // Currently TerminateAuthFactor always succeeds, the error handling code
  // below never triggers.
  if (error.has_value()) {
    LOG(ERROR) << "fp factor fails to terminate a sensor session.";
    return;
  }
  stage_ = Stage::kIdle;
  std::move(callback).Run(GetFactor());
}

CryptohomeFingerprintEngineFactory::CryptohomeFingerprintEngineFactory(
    PrefService* local_state)
    : local_state_(local_state) {}

CryptohomeFingerprintEngineFactory::~CryptohomeFingerprintEngineFactory() =
    default;

AshAuthFactor CryptohomeFingerprintEngineFactory::GetFactor() {
  return AshAuthFactor::kFingerprint;
}

std::unique_ptr<AuthFactorEngine>
CryptohomeFingerprintEngineFactory::CreateEngine(AuthHubMode mode) {
  CHECK(CryptohomeCore::Get());
  return std::make_unique<CryptohomeFingerprintEngine>(*CryptohomeCore::Get(),
                                                       local_state_);
}

}  // namespace ash
