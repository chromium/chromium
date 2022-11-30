// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_

#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefRegistrySimple;

namespace ash::auth {

// The implementation of the AuthFactorConfig service.
// TODO(crbug.com/1327627): This will eventually communicate with cryptohome,
// but it is currently a fake and only maintains a boolean corresponding to the
// current state. No changes are sent to cryptohome. The fake reports that
// cryptohome recovery is supported only if the CryptohomeRecoverySetup feature
// flag is enabled.
class AuthFactorConfig : public mojom::AuthFactorConfig {
 public:
  AuthFactorConfig();
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

 private:
  friend class RecoveryFactorEditor;

  void NotifyFactorObservers(mojom::AuthFactor changed_factor);

  bool recovery_configured_ = false;

  mojo::ReceiverSet<mojom::AuthFactorConfig> receivers_;
  mojo::RemoteSet<mojom::FactorObserver> observers_;
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_H_
