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

// The implementation of the PinFactorEditor mojo service.
class PinFactorEditor : public mojom::PinFactorEditor {
 public:
  PinFactorEditor(AuthFactorConfig*, PinBackendDelegate* pin_backend);
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
  void ObtainContext(
      const std::string& auth_token,
      base::OnceCallback<void(std::unique_ptr<UserContext>)> callback);
  void RemovePinWithContext(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      std::unique_ptr<UserContext> context);
  void OnPinConfigured(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool success);
  void OnPinConfiguredWithContext(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool success,
      std::unique_ptr<UserContext> context);
  void SetPinWithContext(
      const std::string& auth_token,
      const std::string& pin,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      std::unique_ptr<UserContext> context);
  void OnIsPinConfiguredForRemove(
      const AccountId account_id,
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool is_pin_configured);

  raw_ptr<AuthFactorConfig> auth_factor_config_;
  raw_ptr<PinBackendDelegate> pin_backend_;
  mojo::ReceiverSet<mojom::PinFactorEditor> receivers_;
  AuthFactorEditor auth_factor_editor_;
  base::WeakPtrFactory<PinFactorEditor> weak_factory_{this};
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PIN_FACTOR_EDITOR_H_
