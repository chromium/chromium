// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_

#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "chromeos/ash/services/auth_factor_config/quick_unlock_storage_delegate.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user.h"

namespace ash::auth {

// The implementation of the AuthFactorConfig service.
class AuthFactorConfig : public mojom::AuthFactorConfig {
 public:
  explicit AuthFactorConfig(QuickUnlockStorageDelegate*);
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

  // Reload auth factor data from cryptohome and notify factor change
  // observers of the change.
  void NotifyFactorObservers(mojom::AuthFactor changed_factor);

 private:
  raw_ptr<QuickUnlockStorageDelegate> quick_unlock_storage_;
  mojo::ReceiverSet<mojom::AuthFactorConfig> receivers_;
  mojo::RemoteSet<mojom::FactorObserver> observers_;
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_
