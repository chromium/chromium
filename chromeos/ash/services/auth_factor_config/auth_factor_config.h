// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_

#include <optional>

#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::auth {

// The implementation of the AuthFactorConfig service.
class AuthFactorConfig : public mojom::AuthFactorConfig {
 public:
  class TestApi {
   public:
    explicit TestApi(AuthFactorConfig& auth_factor_config)
        : auth_factor_config_(auth_factor_config) {}

    // Injects a callback that gets invoked after knowledge factor
    // is added.
    void SetAddKnowledgeFactorCallback(base::OnceClosure callback) {
      auth_factor_config_->SetAddKnowledgeFactorCallbackForTesting(
          std::move(callback));
    }

    // Instructs AuthFactorConfig not to inform
    // UserDirectoryIntegrityManager about added factors.
    void SetSkipUserIntegrityNotification(bool skip_notification) {
      auth_factor_config_->SetSkipUserIntegrityNotificationForTesting(
          skip_notification);
    }

   private:
    const raw_ref<AuthFactorConfig> auth_factor_config_;
  };

  using AuthFactorSet = base::EnumSet<mojom::AuthFactor,
                                      mojom::AuthFactor::kMinValue,
                                      mojom::AuthFactor::kMaxValue>;

  explicit AuthFactorConfig(QuickUnlockStorageDelegate*,
                            PrefService* local_state);
  ~AuthFactorConfig() override;

  AuthFactorConfig(const AuthFactorConfig&) = delete;
  AuthFactorConfig& operator=(const AuthFactorConfig&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void BindReceiver(mojo::PendingReceiver<mojom::AuthFactorConfig> receiver);

  void ObserveFactorChanges(
      mojo::PendingRemote<mojom::FactorObserver>) override;
  void IsSupported(const std::string& auth_token,
                   mojom::AuthFactor factor,
                   base::OnceCallback<void(bool)>) override;
  void IsConfigured(const std::string& auth_token,
                    mojom::AuthFactor factor,
                    base::OnceCallback<void(bool)>) override;
  void GetManagementType(
      const std::string& auth_token,
      mojom::AuthFactor factor,
      base::OnceCallback<void(mojom::ManagementType)>) override;
  void IsEditable(const std::string& auth_token,
                  mojom::AuthFactor factor,
                  base::OnceCallback<void(bool)>) override;

  // Reload auth factor data from cryptohome and notify factor change observers
  // of the change. This method must be called after successful mutating
  // UserDataAuth calls so that the list of auth factors remains up to date.
  // `context` should be a copy of the user context stored in quick unlock
  // storage. In particular, `context` should contain an authenticated auth
  // session
  void NotifyFactorObserversAfterSuccess(
      AuthFactorSet changed_factor,
      const std::string& auth_token,
      std::unique_ptr<UserContext> context,
      base::OnceCallback<void(mojom::ConfigureResult)> callback);

  // Like NotifyFactorObserversAfterSuccess, but supposed to be called before
  // we return a `kFatalError` result because of a failed mutating UserDataAuth
  // call. This method will reload auth factors and send a change notification
  // to observers for all auth factors.
  // This is useful because a likely source of errors is outdated information
  // about the status of configured auth factors, resulting in an invalid
  // UserDataAuth call. For example, we might think that an auth factor is
  // configured and try to update it. If some other system has removed this
  // auth factor without our knowledge, the update call will fail. By
  // refreshing our information on what auth factors are configured, we can
  // recover so that the user can try again.
  void NotifyFactorObserversAfterFailure(const std::string& auth_token,
                                         std::unique_ptr<UserContext> context,
                                         base::OnceCallback<void()> callback);

  // Called when user is known to have knowledge factor set up.
  void OnUserHasKnowledgeFactor(const UserContext& context);

 private:
  friend class TestApi;

  using FactorStatusCheckOperation =
      base::OnceCallback<void(std::unique_ptr<UserContext>)>;
  using FactorStatusCheckResultCallback = base::OnceCallback<void(bool)>;
  using OnRefreshAuthFactorsConfiguration =
      base::OnceCallback<void(std::unique_ptr<UserContext>)>;

  void ObtainContext(
      const std::string& auth_token,
      base::OnceCallback<void(std::unique_ptr<UserContext>)> callback);
  void IsSupportedWithContext(const std::string& auth_token,
                              mojom::AuthFactor factor,
                              FactorStatusCheckResultCallback callback,
                              std::unique_ptr<UserContext> context);
  void IsConfiguredWithContext(const std::string& auth_token,
                               mojom::AuthFactor factor,
                               FactorStatusCheckResultCallback,
                               std::unique_ptr<UserContext> context);
  void IsEditableWithContext(const std::string& auth_token,
                             mojom::AuthFactor factor,
                             FactorStatusCheckResultCallback,
                             std::unique_ptr<UserContext> context);
  void OnGetAuthFactorsConfiguration(
      AuthFactorSet changed_factors,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      const std::string& auth_token,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);

  void SetAddKnowledgeFactorCallbackForTesting(base::OnceClosure callback);
  void SetSkipUserIntegrityNotificationForTesting(bool skip_notification);

  void RefreshAuthFactorsConfiguration(
      std::unique_ptr<UserContext> context,
      OnRefreshAuthFactorsConfiguration continuation);
  void OnAuthFactorConfigurationRefreshed(
      OnRefreshAuthFactorsConfiguration continuation,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);
  void MaybeRetryFactorStatusCheckOnConfigRefresh(
      FactorStatusCheckOperation,
      FactorStatusCheckResultCallback,
      const std::string& auth_token,
      std::unique_ptr<UserContext> context);

  raw_ptr<QuickUnlockStorageDelegate> quick_unlock_storage_;
  // This instance is held by browser process (see in_process_instances)
  // as well as local_state_, so they should be local state would become
  // invalid quite late in the flow, by that time it should not be possible
  // to interact with AuthFactorConfig.
  raw_ptr<PrefService, DisableDanglingPtrDetection> local_state_;
  mojo::ReceiverSet<mojom::AuthFactorConfig> receivers_;
  mojo::RemoteSet<mojom::FactorObserver> observers_;
  AuthFactorEditor auth_factor_editor_;

  // Used for testing, invoked when a knowledge factor is added.
  base::OnceClosure add_knowledge_factor_callback_;
  std::optional<bool> skip_user_integrity_notification_;

  base::WeakPtrFactory<AuthFactorConfig> weak_factory_{this};
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_
