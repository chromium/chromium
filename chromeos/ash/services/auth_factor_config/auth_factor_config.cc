// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::auth {

AuthFactorConfig::AuthFactorConfig() = default;

AuthFactorConfig::~AuthFactorConfig() = default;

// static
void AuthFactorConfig::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ash::prefs::kRecoveryFactorBehavior, true);
}

void AuthFactorConfig::BindReceiver(
    mojo::PendingReceiver<mojom::AuthFactorConfig> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AuthFactorConfig::ObserveFactorChanges(
    mojo::PendingRemote<mojom::FactorObserver> observer) {
  observers_.Add(std::move(observer));
}

void AuthFactorConfig::NotifyFactorObservers(mojom::AuthFactor changed_factor) {
  for (auto& observer : observers_)
    observer->OnFactorChanged(changed_factor);
}

void AuthFactorConfig::IsSupported(const std::string& auth_token,
                                   mojom::AuthFactor factor,
                                   base::OnceCallback<void(bool)> callback) {
  VLOG(1) << "AuthFactorConfig::IsSupported is a fake";
  std::move(callback).Run(features::IsCryptohomeRecoverySetupEnabled());
}

void AuthFactorConfig::IsConfigured(const std::string& auth_token,
                                    mojom::AuthFactor factor,
                                    base::OnceCallback<void(bool)> callback) {
  if (!features::IsCryptohomeRecoverySetupEnabled()) {
    // Log always, crash on debug builds.
    LOG(ERROR) << "AuthFactorConfig::IsConfigured is a fake";
    NOTIMPLEMENTED();
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(recovery_configured_);
}

void AuthFactorConfig::GetManagementType(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(mojom::ManagementType)> callback) {
  if (!features::IsCryptohomeRecoverySetupEnabled()) {
    // Log always, crash on debug builds.
    LOG(ERROR) << "AuthFactorConfig::GetManagementType is a fake";
    NOTIMPLEMENTED();
    std::move(callback).Run(mojom::ManagementType::kNone);
    return;
  }

  std::move(callback).Run(mojom::ManagementType::kNone);
}

void AuthFactorConfig::IsEditable(const std::string& auth_token,
                                  mojom::AuthFactor factor,
                                  base::OnceCallback<void(bool)> callback) {
  if (!features::IsCryptohomeRecoverySetupEnabled()) {
    // Log always, crash on debug builds.
    LOG(ERROR) << "AuthFactorConfig::IsEditable is a fake";
    NOTIMPLEMENTED();
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

}  // namespace ash::auth
