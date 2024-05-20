// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/prefs_pin_engine.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace ash {

PrefsPinEngine::PrefsPinEngine(CryptohomeCore& core, PrefService& pref_service)
    : core_(&core), pref_service_(&pref_service) {}

PrefsPinEngine::~PrefsPinEngine() = default;

void PrefsPinEngine::PerformPinAttempt(const std::string& raw_pin) {
  // TODO(b/271263584): Do not allow the use of PIN if the system supports
  // proper hardware-backed PINs.

  // Ignore the attempt if use is not currently enabled.
  if (usage_allowed_ != UsageAllowed::kEnabled) {
    LOG(ERROR) << "Ignoring legacy PIN attempt as factor is disabled";
    return;
  }
  // Ignore the attempt if the PIN is locked out.
  if (IsLockedOut()) {
    LOG(ERROR) << "Ignoring legacy PIN attempt as factor is locked out";
    return;
  }
  // Extract the stored PIN & salt. Ignore the attempt if they don't exist.
  const std::string salt = pref_service_->GetString(prefs::kQuickUnlockPinSalt);
  if (salt.empty()) {
    LOG(ERROR) << "Ignoring legacy PIN attempt as user has no PIN salt";
    return;
  }
  const std::string secret =
      pref_service_->GetString(prefs::kQuickUnlockPinSecret);
  if (secret.empty()) {
    LOG(ERROR) << "Ignoring legacy PIN attempt as user has no PIN secret";
    return;
  }
  // Attempt authentication. Take the raw PIN input, do a salted key derivation,
  // and compare it to the stored secret.
  observer_->OnFactorAttempt(GetFactor());
  bool auth_success = false;
  unlock_attempt_count_ += 1;
  Key key(raw_pin);
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
  if (key.GetSecret() == secret) {
    auth_success = true;
  } else if (IsLockedOut()) {
    // If the attempt failed and we are now locked out, signal this.
    observer_->OnLockoutChanged(GetFactor());
  }
  observer_->OnFactorAttemptResult(GetFactor(), auth_success);
}

AshAuthFactor PrefsPinEngine::GetFactor() const {
  return AshAuthFactor::kLegacyPin;
}

void PrefsPinEngine::InitializeCommon(CommonInitCallback callback) {
  core_->WaitForService(base::BindOnce(&PrefsPinEngine::OnCryptohomeReady,
                                       weak_factory_.GetWeakPtr(),
                                       std::move(callback)));
}

void PrefsPinEngine::ShutdownCommon(ShutdownCallback callback) {
  std::move(callback).Run(GetFactor());
}

void PrefsPinEngine::StartAuthFlow(const AccountId& account,
                                   AuthPurpose purpose,
                                   FactorEngineObserver* observer) {
  observer_ = observer;
  usage_allowed_ = UsageAllowed::kDisabled;
  // TODO(b/271263584): Add a way to start a session with the core without
  // actually requiring an underlying session. Currently this is the only way to
  // ensure that a proper user context exists.
  core_->StartAuthSession({account, purpose}, this);
}

void PrefsPinEngine::UpdateObserver(FactorEngineObserver* observer) {
  observer_ = observer;
}

void PrefsPinEngine::CleanUp(CleanupCallback callback) {}

void PrefsPinEngine::StopAuthFlow(ShutdownCallback callback) {
  observer_ = nullptr;
  std::move(callback).Run(GetFactor());
}

AuthProofToken PrefsPinEngine::StoreAuthenticationContext() {
  return core_->StoreAuthenticationContext();
}

void PrefsPinEngine::SetUsageAllowed(UsageAllowed usage) {
  usage_allowed_ = usage;
}

bool PrefsPinEngine::IsDisabledByPolicy() {
  return false;
}

bool PrefsPinEngine::IsLockedOut() {
  return unlock_attempt_count_ >= kMaximumUnlockAttempts;
}

bool PrefsPinEngine::IsFactorSpecificRestricted() {
  return false;
}

void PrefsPinEngine::OnSuccessfulAuthentiation() {
  unlock_attempt_count_ = 0;
}

void PrefsPinEngine::OnCryptohomeAuthSessionStarted() {}

void PrefsPinEngine::OnAuthSessionStartFailure() {}

void PrefsPinEngine::OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) {}

void PrefsPinEngine::OnCryptohomeAuthSessionFinished() {}

void PrefsPinEngine::OnCryptohomeReady(CommonInitCallback callback,
                                       bool service_available) {
  if (!service_available) {
    LOG(ERROR) << "cryptohomed not started, Factor "
               << static_cast<int>(GetFactor()) << " is not available";
    return;
  }
  // TODO(b/271263584): check if legacy PIN is allowed on this system.
  std::move(callback).Run(GetFactor());
}

PrefsPinEngineFactory::PrefsPinEngineFactory(PrefService& local_state)
    : local_state_(&local_state) {}

AshAuthFactor PrefsPinEngineFactory::GetFactor() {
  return AshAuthFactor::kLegacyPin;
}

std::unique_ptr<AuthFactorEngine> PrefsPinEngineFactory::CreateEngine(
    AuthHubMode mode) {
  // The legacy PIN engine is only available in-session.
  if (mode == AuthHubMode::kInSession) {
    auto* core = CryptohomeCore::Get();
    CHECK(core);
    return std::make_unique<PrefsPinEngine>(*core, *local_state_);
  }
  return nullptr;
}

}  // namespace ash
