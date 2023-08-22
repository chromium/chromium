// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

AuthFactorConfig::AuthFactorConfig(
    QuickUnlockStorageDelegate* quick_unlock_storage)
    : quick_unlock_storage_(quick_unlock_storage),
      auth_factor_editor_(UserDataAuthClient::Get()) {
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

void AuthFactorConfig::NotifyFactorObserversAfterSuccess(
    AuthFactorSet changed_factors,
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  CHECK(context);

  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(&AuthFactorConfig::OnGetAuthFactorsConfiguration,
                     weak_factory_.GetWeakPtr(), changed_factors,
                     std::move(callback), auth_token));
}

void AuthFactorConfig::NotifyFactorObserversAfterFailure(
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    base::OnceCallback<void()> callback) {
  CHECK(context);

  // The original callback, but with an additional ignored parameter so that we
  // can pass it to `OnGetAuthFactorsConfiguration`.
  base::OnceCallback<void(mojom::ConfigureResult)> ignore_param_callback =
      base::BindOnce([](base::OnceCallback<void()> callback,
                        mojom::ConfigureResult) { std::move(callback).Run(); },
                     std::move(callback));

  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(&AuthFactorConfig::OnGetAuthFactorsConfiguration,
                     weak_factory_.GetWeakPtr(), AuthFactorSet::All(),
                     std::move(ignore_param_callback), auth_token));
}

void AuthFactorConfig::IsSupported(const std::string& auth_token,
                                   mojom::AuthFactor factor,
                                   base::OnceCallback<void(bool)> callback) {
  UserContext* user_context;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
      LOG(ERROR) << "Invalid or expired auth token";
      std::move(callback).Run(false);
      return;
    }
    user_context = ash::AuthSessionStorage::Get()->Peek(auth_token);
  } else {
    const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
    user_context = quick_unlock_storage_->GetUserContext(user, auth_token);
    if (!user_context) {
      LOG(ERROR) << "Invalid auth token";
      std::move(callback).Run(false);
      return;
    }
  }
  const cryptohome::AuthFactorsSet cryptohome_supported_factors =
      user_context->GetAuthFactorsConfiguration().get_supported_factors();

  switch (factor) {
    case mojom::AuthFactor::kRecovery: {
      if (!features::IsCryptohomeRecoveryEnabled()) {
        std::move(callback).Run(false);
        return;
      }

      std::move(callback).Run(cryptohome_supported_factors.Has(
          cryptohome::AuthFactorType::kRecovery));
      return;
    }
    case mojom::AuthFactor::kPin: {
      std::move(callback).Run(
          cryptohome_supported_factors.Has(cryptohome::AuthFactorType::kPin));
      return;
    }
    case mojom::AuthFactor::kGaiaPassword: {
      std::move(callback).Run(true);
      return;
    }
    case mojom::AuthFactor::kLocalPassword: {
      std::move(callback).Run(
          features::IsPasswordlessGaiaEnabledForConsumers());
      return;
    }
  }

  NOTREACHED();
}

void AuthFactorConfig::IsConfigured(const std::string& auth_token,
                                    mojom::AuthFactor factor,
                                    base::OnceCallback<void(bool)> callback) {
  UserContext* user_context;
  const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();

  if (ash::features::ShouldUseAuthSessionStorage()) {
    if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
      LOG(ERROR) << "Invalid or expired auth token";
      std::move(callback).Run(false);
      return;
    }
    user_context = ash::AuthSessionStorage::Get()->Peek(auth_token);
  } else {
    user_context = quick_unlock_storage_->GetUserContext(user, auth_token);
    if (!user_context) {
      LOG(ERROR) << "Invalid auth token";
      std::move(callback).Run(false);
      return;
    }
  }
  const auto& config = user_context->GetAuthFactorsConfiguration();

  switch (factor) {
    case mojom::AuthFactor::kRecovery: {
      DCHECK(features::IsCryptohomeRecoveryEnabled());
      std::move(callback).Run(
          config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery));
      return;
    }
    case mojom::AuthFactor::kPin: {
      // We have to consider both cryptohome based PIN and legacy pref PIN.
      if (config.HasConfiguredFactor(cryptohome::AuthFactorType::kPin)) {
        std::move(callback).Run(true);
        return;
      }

      const PrefService* prefs = quick_unlock_storage_->GetPrefService(*user);
      if (!prefs) {
        LOG(ERROR) << "No pref service for user";
        std::move(callback).Run(false);
        return;
      }

      const bool has_prefs_pin =
          !prefs->GetString(prefs::kQuickUnlockPinSecret).empty() &&
          !prefs->GetString(prefs::kQuickUnlockPinSalt).empty();

      std::move(callback).Run(has_prefs_pin);
      return;
    }
    case mojom::AuthFactor::kGaiaPassword: {
      const cryptohome::AuthFactor* password_factor =
          config.FindFactorByType(cryptohome::AuthFactorType::kPassword);
      if (!password_factor) {
        std::move(callback).Run(false);
        return;
      }

      std::move(callback).Run(IsGaiaPassword(*password_factor));
      return;
    }
    case mojom::AuthFactor::kLocalPassword: {
      const cryptohome::AuthFactor* password_factor =
          config.FindFactorByType(cryptohome::AuthFactorType::kPassword);
      if (!password_factor) {
        std::move(callback).Run(false);
        return;
      }

      std::move(callback).Run(IsLocalPassword(*password_factor));
      return;
    }
  }

  NOTREACHED();
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
    case mojom::AuthFactor::kGaiaPassword:
    case mojom::AuthFactor::kLocalPassword: {
      // There are currently no policies related to Gaia/local passwords.
      std::move(callback).Run(mojom::ManagementType::kNone);
      return;
    }
  }

  NOTREACHED();
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

      if (prefs->IsUserModifiablePreference(prefs::kRecoveryFactorBehavior)) {
        std::move(callback).Run(true);
        return;
      }

      UserContext* user_context;
      if (ash::features::ShouldUseAuthSessionStorage()) {
        if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
          LOG(ERROR) << "Invalid or expired auth token";
          std::move(callback).Run(false);
          return;
        }
        user_context = ash::AuthSessionStorage::Get()->Peek(auth_token);
      } else {
        user_context = quick_unlock_storage_->GetUserContext(user, auth_token);
        if (!user_context) {
          LOG(ERROR) << "Invalid auth token";
          std::move(callback).Run(false);
          return;
        }
      }
      const auto& config = user_context->GetAuthFactorsConfiguration();
      const bool is_configured =
          config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery);

      if (is_configured != prefs->GetBoolean(prefs::kRecoveryFactorBehavior)) {
        std::move(callback).Run(true);
        return;
      }

      std::move(callback).Run(false);
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
    case mojom::AuthFactor::kGaiaPassword: {
      // TODO(b/290916811): Decide upon when to return true here. For now we
      // don't allow edits or removal of Gaia passwords once they're
      // configured, so we always return false.
      std::move(callback).Run(false);
      return;
    }
    case mojom::AuthFactor::kLocalPassword: {
      std::move(callback).Run(true);
      return;
    }
  }

  NOTREACHED();
}

void AuthFactorConfig::OnGetAuthFactorsConfiguration(
    AuthFactorSet changed_factors,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (ash::features::ShouldUseAuthSessionStorage()) {
    ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
  }
  if (error.has_value()) {
    LOG(ERROR) << "Refreshing list of configured auth factors failed, code "
               << error->get_cryptohome_code();
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }
  if (!ash::features::ShouldUseAuthSessionStorage()) {
    const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
    quick_unlock_storage_->SetUserContext(user, std::move(context));
  }

  std::move(callback).Run(mojom::ConfigureResult::kSuccess);

  for (auto& observer : observers_) {
    for (const auto changed_factor : changed_factors) {
      observer->OnFactorChanged(changed_factor);
    }
  }
}

}  // namespace ash::auth
