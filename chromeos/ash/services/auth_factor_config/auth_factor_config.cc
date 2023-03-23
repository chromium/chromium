// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

AuthFactorConfig::AuthFactorConfig(
    QuickUnlockStorageDelegate* quick_unlock_storage)
    : quick_unlock_storage_(quick_unlock_storage) {
  DCHECK(quick_unlock_storage_);
}

AuthFactorConfig::~AuthFactorConfig() = default;

// static
void AuthFactorConfig::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ash::prefs::kRecoveryFactorBehavior, false);
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
  if (!features::IsCryptohomeRecoveryEnabled()) {
    std::move(callback).Run(false);
    return;
  }

  const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
  auto* user_context = quick_unlock_storage_->GetUserContext(user, auth_token);
  if (!user_context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(false);
    return;
  }

  const bool is_supported_by_cryptohome =
      user_context->GetAuthFactorsConfiguration().get_supported_factors().Has(
          cryptohome::AuthFactorType::kRecovery);
  std::move(callback).Run(is_supported_by_cryptohome);
}

void AuthFactorConfig::IsConfigured(const std::string& auth_token,
                                    mojom::AuthFactor factor,
                                    base::OnceCallback<void(bool)> callback) {
  DCHECK(features::IsCryptohomeRecoveryEnabled());

  if (factor != mojom::AuthFactor::kRecovery) {
    LOG(ERROR) << "AuthFactorConfig::IsConfigured supports recovery only";
    NOTIMPLEMENTED();
    std::move(callback).Run(false);
    return;
  }

  const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
  auto* user_context = quick_unlock_storage_->GetUserContext(user, auth_token);
  if (!user_context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(false);
    return;
  }

  const auto& config = user_context->GetAuthFactorsConfiguration();
  const bool is_configured =
      config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery);
  std::move(callback).Run(is_configured);
}

void AuthFactorConfig::GetManagementType(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(mojom::ManagementType)> callback) {
  switch (factor) {
    case mojom::AuthFactor::kRecovery: {
      DCHECK(features::IsCryptohomeRecoveryEnabled());
      const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
      CHECK(user);
      const PrefService* prefs = quick_unlock_storage_->GetPrefService(*user);
      CHECK(prefs);
      const mojom::ManagementType result =
          prefs->IsManagedPreference(prefs::kRecoveryFactorBehavior)
              ? mojom::ManagementType::kUser
              : mojom::ManagementType::kNone;

      std::move(callback).Run(result);
      return;
    }
    case mojom::AuthFactor::kPin: {
      const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
      CHECK(user);
      const PrefService* prefs = quick_unlock_storage_->GetPrefService(*user);
      CHECK(prefs);

      if (prefs->IsManagedPreference(prefs::kQuickUnlockModeAllowlist) ||
          prefs->IsManagedPreference(prefs::kWebAuthnFactors)) {
        std::move(callback).Run(mojom::ManagementType::kUser);
      } else {
        std::move(callback).Run(mojom::ManagementType::kNone);
      }
      return;
    }
  }
}

void AuthFactorConfig::IsEditable(const std::string& auth_token,
                                  mojom::AuthFactor factor,
                                  base::OnceCallback<void(bool)> callback) {
  switch (factor) {
    case mojom::AuthFactor::kRecovery: {
      DCHECK(features::IsCryptohomeRecoveryEnabled());
      const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
      CHECK(user);
      const PrefService* prefs = quick_unlock_storage_->GetPrefService(*user);
      CHECK(prefs);

      std::move(callback).Run(
          prefs->IsUserModifiablePreference(prefs::kRecoveryFactorBehavior));
      return;
    }
    case mojom::AuthFactor::kPin: {
      const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
      CHECK(user);
      const PrefService* prefs = quick_unlock_storage_->GetPrefService(*user);
      CHECK(prefs);

      // Lists of factors that are allowed for some purpose.
      const base::Value::List* pref_lists[] = {
          &prefs->GetList(prefs::kQuickUnlockModeAllowlist),
          &prefs->GetList(prefs::kWebAuthnFactors),
      };

      // Values in factor lists that match PINs.
      const base::Value pref_list_values[] = {
          base::Value("all"),
          base::Value("PIN"),
      };

      for (const auto* pref_list : pref_lists) {
        for (const auto& pref_list_value : pref_list_values) {
          if (base::Contains(*pref_list, pref_list_value)) {
            std::move(callback).Run(true);
            return;
          }
        }
      }

      std::move(callback).Run(false);
      return;
    }
  }
}

}  // namespace ash::auth
