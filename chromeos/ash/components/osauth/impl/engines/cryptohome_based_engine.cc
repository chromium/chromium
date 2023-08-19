// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/cryptohome_based_engine.h"

#include "base/functional/bind.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

CryptohomeBasedEngine::CryptohomeBasedEngine(CryptohomeCore& core,
                                             AshAuthFactor factor)
    : core_(&core), factor_(factor) {}

CryptohomeBasedEngine::~CryptohomeBasedEngine() = default;

AshAuthFactor CryptohomeBasedEngine::GetFactor() const {
  return factor_;
}

void CryptohomeBasedEngine::InitializeCommon(CommonInitCallback callback) {
  core_->WaitForService(
      base::BindOnce(&CryptohomeBasedEngine::OnCryptohomeReady,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CryptohomeBasedEngine::OnCryptohomeReady(CommonInitCallback callback,
                                              bool service_available) {
  if (!service_available) {
    LOG(ERROR) << "cryptohomed not started, Factor "
               << static_cast<int>(GetFactor()) << " is not available";
    return;
  }
  std::move(callback).Run(GetFactor());
}

void CryptohomeBasedEngine::ShutdownCommon(ShutdownCallback callback) {
  std::move(callback).Run(GetFactor());
}

void CryptohomeBasedEngine::StartAuthFlow(const AccountId& account,
                                          AuthPurpose purpose,
                                          FactorEngineObserver* observer) {
  observer_ = observer;
  usage_allowed_ = UsageAllowed::kDisabled;
  core_->StartAuthSession({account, purpose}, this);
}

void CryptohomeBasedEngine::OnCryptohomeAuthSessionStarted() {
  key_ref_ = LookUpFactor(*core_->GetCurrentContext());
  observer_->OnFactorPresenceChecked(GetFactor(), key_ref_.has_value());
}

void CryptohomeBasedEngine::OnAuthSessionStartFailure() {
  observer_->OnCriticalError(GetFactor());
}

void CryptohomeBasedEngine::UpdateObserver(FactorEngineObserver* observer) {
  observer_ = observer;
}

void CryptohomeBasedEngine::StopAuthFlow(ShutdownCallback callback) {
  CHECK(shutdown_callback_.is_null());
  shutdown_callback_ = std::move(callback);
  observer_ = nullptr;
  // Note: With FakeUserDataAuthClient next call might result in
  // shutdown callback being called synchronously (and `this` being deleted).
  core_->EndAuthSession(this);
}

AuthProofToken CryptohomeBasedEngine::StoreAuthenticationContext() {
  return core_->StoreAuthenticationContext();
}

void CryptohomeBasedEngine::OnCryptohomeAuthSessionFinished() {
  CHECK(!shutdown_callback_.is_null());
  std::move(shutdown_callback_).Run(GetFactor());
}

void CryptohomeBasedEngine::SetUsageAllowed(UsageAllowed usage) {
  usage_allowed_ = usage;
}

}  // namespace ash
