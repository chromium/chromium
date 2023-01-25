// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PIN_FACTOR_EDITOR_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PIN_FACTOR_EDITOR_H_

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::auth {

class AuthFactorConfig;

// The implementation of the PinFactorEditor service.
// TODO(crbug.com/1327627): This is currently a fake and only flips a boolean
// corresponding to the current state. No changes are sent to cryptohome.
// Clients may use this API only if the CryptohomeRecoverySetup feature flag is
// enabled.
class PinFactorEditor : public mojom::PinFactorEditor {
 public:
  PinFactorEditor(AuthFactorConfig*,
                  PinBackendDelegate* pin_backend,
                  QuickUnlockStorageDelegate* storage);
  ~PinFactorEditor() override;

  PinFactorEditor(const PinFactorEditor&) = delete;
  PinFactorEditor& operator=(const PinFactorEditor&) = delete;

  void SetPin(
      const std::string& auth_token,
      const std::string& pin,
      base::OnceCallback<void(mojom::ConfigureResult)> callback) override;
  void RemovePin(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback) override;

  void BindReceiver(mojo::PendingReceiver<mojom::PinFactorEditor> receiver);

 private:
  void OnPinConfigured(
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool success);

  raw_ptr<AuthFactorConfig> auth_factor_config_;
  raw_ptr<PinBackendDelegate> pin_backend_;
  raw_ptr<QuickUnlockStorageDelegate> quick_unlock_storage_;
  mojo::ReceiverSet<mojom::PinFactorEditor> receivers_;
  AuthFactorEditor auth_factor_editor_;
  base::WeakPtrFactory<PinFactorEditor> weak_factory_{this};
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PIN_FACTOR_EDITOR_H_
