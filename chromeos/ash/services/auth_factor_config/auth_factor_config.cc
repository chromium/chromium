// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_directory_integrity_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

AuthFactorConfig::AuthFactorConfig(
    QuickUnlockStorageDelegate* quick_unlock_storage,
    PrefService* local_state)
    : quick_unlock_storage_(quick_unlock_storage),
      local_state_(local_state),
      auth_factor_editor_(UserDataAuthClient::Get()) {
  CHECK(quick_unlock_storage_);
  CHECK(local_state_);
}

AuthFactorConfig::~AuthFactorConfig() = default;

// static
void AuthFactorConfig::RegisterPrefs(PrefRegistrySimple* registry) {
  const bool default_for_enterprise = base::FeatureList::IsEnabled(
      ash::features::kCryptohomeRecoveryByDefaultForEnterprise);
  registry->RegisterBooleanPref(ash::prefs::kRecoveryFactorBehavior,
                                default_for_enterprise);
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

void AuthFactorConfig::OnUserHasKnowledgeFactor(const UserContext& context) {
  if (add_knowledge_factor_callback_) {
    std::move(add_knowledge_factor_callback_).Run();
  }
  if (skip_user_integrity_notification_.value_or(false)) {
    return;
  }
  user_manager::UserDirectoryIntegrityManager(local_state_).ClearPrefs();
}

void AuthFactorConfig::IsSupported(const std::string& auth_token,
                                   mojom::AuthFactor factor,
                                   base::OnceCallback<void(bool)> callback) {
  ObtainContext(auth_token,
                base::BindOnce(&AuthFactorConfig::IsSupportedWithContext,
                               weak_factory_.GetWeakPtr(), auth_token, factor,
                               std::move(callback)));
}
void AuthFactorConfig::IsSupportedWithContext(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid or expired auth token";
    std::move(callback).Run(false);
    return;
  }

  if (context->HasAuthFactorsConfiguration()) {
    const cryptohome::AuthFactorsSet cryptohome_supported_factors =
        context->GetAuthFactorsConfiguration().get_supported_factors();

    ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));

    switch (factor) {
      case mojom::AuthFactor::kRecovery: {
        std::move(callback).Run(cryptohome_supported_factors.Has(
            cryptohome::AuthFactorType::kRecovery));
        return;
      }
      case mojom::AuthFactor::kPin: {
        std::move(callback).Run(
            cryptohome_supported_factors.Has(cryptohome::AuthFactorType::kPin));
        return;
      }
      case mojom::AuthFactor::kLocalPassword:
      case mojom::AuthFactor::kGaiaPassword: {
        std::move(callback).Run(true);
        return;
      }
    }

    NOTREACHED_IN_MIGRATION();
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  FactorStatusCheckOperation retry = base::BindOnce(
      &AuthFactorConfig::IsSupportedWithContext, weak_factory_.GetWeakPtr(),
      auth_token, factor, std::move(split_callback.first));

  RefreshAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(
          &AuthFactorConfig::MaybeRetryFactorStatusCheckOnConfigRefresh,
          weak_factory_.GetWeakPtr(), std::move(retry),
          std::move(split_callback.second), auth_token));
}

void AuthFactorConfig::IsConfigured(const std::string& auth_token,
                                    mojom::AuthFactor factor,
                                    base::OnceCallback<void(bool)> callback) {
  ObtainContext(auth_token,
                base::BindOnce(&AuthFactorConfig::IsConfiguredWithContext,
                               weak_factory_.GetWeakPtr(), auth_token, factor,
                               std::move(callback)));
}

void AuthFactorConfig::IsConfiguredWithContext(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid or expired auth token";
    std::move(callback).Run(false);
    return;
  }

  if (context->HasAuthFactorsConfiguration()) {
    const auto& config = context->GetAuthFactorsConfiguration();
    ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));

    switch (factor) {
      case mojom::AuthFactor::kRecovery: {
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
        const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
        if (!user) {
          LOG(ERROR) << "No logged in user";
          std::move(callback).Run(false);
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

    NOTREACHED_IN_MIGRATION();
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  FactorStatusCheckOperation retry = base::BindOnce(
      &AuthFactorConfig::IsConfiguredWithContext, weak_factory_.GetWeakPtr(),
      auth_token, factor, std::move(split_callback.first));

  RefreshAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(
          &AuthFactorConfig::MaybeRetryFactorStatusCheckOnConfigRefresh,
          weak_factory_.GetWeakPtr(), std::move(retry),
          std::move(split_callback.second), auth_token));
}

void AuthFactorConfig::GetManagementType(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(mojom::ManagementType)> callback) {
  switch (factor) {
    case mojom::AuthFactor::kRecovery: {
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

  NOTREACHED_IN_MIGRATION();
}

void AuthFactorConfig::IsEditable(const std::string& auth_token,
                                  mojom::AuthFactor factor,
                                  base::OnceCallback<void(bool)> callback) {
  ObtainContext(auth_token,
                base::BindOnce(&AuthFactorConfig::IsEditableWithContext,
                               weak_factory_.GetWeakPtr(), auth_token, factor,
                               std::move(callback)));
}
void AuthFactorConfig::IsEditableWithContext(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid or expired auth token";
    std::move(callback).Run(false);
    return;
  }

  if (context->HasAuthFactorsConfiguration()) {
    const auto& config = context->GetAuthFactorsConfiguration();

    ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));

    switch (factor) {
      case mojom::AuthFactor::kRecovery: {
        const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
        CHECK(user);

        const PrefService* prefs = quick_unlock_storage_->GetPrefService(*user);
        CHECK(prefs);

        if (prefs->IsUserModifiablePreference(prefs::kRecoveryFactorBehavior)) {
          std::move(callback).Run(true);
          return;
        }

        const bool is_configured =
            config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery);

        if (is_configured !=
            prefs->GetBoolean(prefs::kRecoveryFactorBehavior)) {
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

    NOTREACHED_IN_MIGRATION();
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  FactorStatusCheckOperation retry = base::BindOnce(
      &AuthFactorConfig::IsConfiguredWithContext, weak_factory_.GetWeakPtr(),
      auth_token, factor, std::move(split_callback.first));

  RefreshAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(
          &AuthFactorConfig::MaybeRetryFactorStatusCheckOnConfigRefresh,
          weak_factory_.GetWeakPtr(), std::move(retry),
          std::move(split_callback.second), auth_token));
}

void AuthFactorConfig::ObtainContext(
    const std::string& auth_token,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> callback) {
  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    std::move(callback).Run(nullptr);
    return;
  }
  ash::AuthSessionStorage::Get()->BorrowAsync(FROM_HERE, auth_token,
                                              std::move(callback));
}

void AuthFactorConfig::OnGetAuthFactorsConfiguration(
    AuthFactorSet changed_factors,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  bool has_knowledge_factor =
      context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kPassword) ||
      context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kPin);

  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));

  if (error.has_value()) {
    LOG(ERROR) << "Refreshing list of configured auth factors failed, code "
               << error->get_cryptohome_code();
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }

  if (has_knowledge_factor) {
    OnUserHasKnowledgeFactor(*context);
  }

  std::move(callback).Run(mojom::ConfigureResult::kSuccess);

  for (auto& observer : observers_) {
    for (const auto changed_factor : changed_factors) {
      observer->OnFactorChanged(changed_factor);
    }
  }
}

void AuthFactorConfig::SetAddKnowledgeFactorCallbackForTesting(
    base::OnceClosure callback) {
  add_knowledge_factor_callback_ = std::move(callback);
}

void AuthFactorConfig::SetSkipUserIntegrityNotificationForTesting(
    bool skip_notification) {
  skip_user_integrity_notification_ = skip_notification;
}

void AuthFactorConfig::RefreshAuthFactorsConfiguration(
    std::unique_ptr<UserContext> context,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> continuation) {
  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(&AuthFactorConfig::OnAuthFactorConfigurationRefreshed,
                     weak_factory_.GetWeakPtr(), std::move(continuation)));
}

void AuthFactorConfig::OnAuthFactorConfigurationRefreshed(
    base::OnceCallback<void(std::unique_ptr<UserContext>)> continuation,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Refreshing list of configured auth factors failed, code "
               << error->get_cryptohome_code();
  }

  std::move(continuation).Run(std::move(context));
}

void AuthFactorConfig::MaybeRetryFactorStatusCheckOnConfigRefresh(
    FactorStatusCheckOperation retry_operation,
    FactorStatusCheckResultCallback callback,
    const std::string& auth_token,
    std::unique_ptr<UserContext> context) {
  if (context->HasAuthFactorsConfiguration()) {
    std::move(retry_operation).Run(std::move(context));
    return;
  }

  // We don't have `AuthFactorsConfiguration`, and we have already retried
  // getting them, without success. Assume the factor is unavailable.
  LOG(ERROR) << "Could not obtain auth factor configuration, assuming factor "
                "unavailable";
  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
  std::move(callback).Run(false);
}

}  // namespace ash::auth
