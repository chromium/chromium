// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_RECOVERY_FACTOR_EDITOR_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_RECOVERY_FACTOR_EDITOR_H_

#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::auth {

class AuthFactorConfig;

// The implementation of the RecoveryFactorEditor service.
// TODO(crbug.com/1327627): This is currently a fake and only flips a boolean
// corresponding to the current state. No changes are sent to cryptohome.
// Clients may use this API only if the CryptohomeRecoverySetup feature flag is
// enabled.
class RecoveryFactorEditor : public mojom::RecoveryFactorEditor {
 public:
  explicit RecoveryFactorEditor(AuthFactorConfig*);
  ~RecoveryFactorEditor() override;

  RecoveryFactorEditor(const RecoveryFactorEditor&) = delete;
  RecoveryFactorEditor& operator=(const RecoveryFactorEditor&) = delete;

  void BindReceiver(
      mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver);

  void Configure(const std::string& auth_token,
                 bool enabled,
                 base::OnceCallback<void(ConfigureResult)>) override;

 private:
  raw_ptr<AuthFactorConfig> auth_factor_config_;
  mojo::ReceiverSet<mojom::RecoveryFactorEditor> receivers_;
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_RECOVERY_FACTOR_EDITOR_H_
