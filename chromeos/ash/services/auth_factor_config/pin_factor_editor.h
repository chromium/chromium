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
  void UpdatePin(
      const std::string& auth_token,
      const std::string& pin,
      base::OnceCallback<void(mojom::ConfigureResult)> callback) override;
  void RemovePin(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback) override;
  void GetConfiguredPinFactor(
      const std::string& auth_token,
      base::OnceCallback<void(std::optional<mojom::AuthFactor>)> callback)
      override;

  void BindReceiver(mojo::PendingReceiver<mojom::PinFactorEditor> receiver);

 private:
  using AuthFactorSet = base::EnumSet<mojom::AuthFactor,
                                      mojom::AuthFactor::kMinValue,
                                      mojom::AuthFactor::kMaxValue>;

  void ObtainContext(
      const std::string& auth_token,
      base::OnceCallback<void(std::unique_ptr<UserContext>)> callback);

  void OnRemovePinConfigured(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      AuthFactorSet factors);
  void OnRemovePinConfiguredWithContext(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      mojom::AuthFactor factor,
      std::unique_ptr<UserContext> context);

  void OnPinRemove(const std::string& auth_token,
                   mojom::AuthFactor factor,
                   base::OnceCallback<void(mojom::ConfigureResult)> callback,
                   bool success);

  void OnPinRemoveWithContext(
      const std::string& auth_token,
      mojom::AuthFactor factor,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool success,
      std::unique_ptr<UserContext> context);

  void SetPinWithContext(
      const std::string& auth_token,
      const std::string& pin,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      std::unique_ptr<UserContext> context);
  void OnPinSet(const std::string& auth_token,
                base::OnceCallback<void(mojom::ConfigureResult)> callback,
                bool success);
  void OnPinSetWithContext(
      const std::string& auth_token,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool success,
      std::unique_ptr<UserContext> context);

  void UpdatePinWithContext(
      const std::string& auth_token,
      const std::string& pin,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      std::unique_ptr<UserContext> context);
  void OnUpdatePinConfigured(
      const std::string& auth_token,
      mojom::AuthFactor old_pin_factor_type,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool success);
  void OnUpdatePinConfiguredWithContext(
      const std::string& auth_token,
      mojom::AuthFactor old_pin_factor_type,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool success,
      std::unique_ptr<UserContext> context);

  void GetConfiguredPinFactorResponse(
      base::OnceCallback<void(std::optional<mojom::AuthFactor>)> callback,
      AuthFactorSet factors);

  raw_ptr<AuthFactorConfig> auth_factor_config_;
  raw_ptr<PinBackendDelegate> pin_backend_;
  mojo::ReceiverSet<mojom::PinFactorEditor> receivers_;
  AuthFactorEditor auth_factor_editor_;
  base::WeakPtrFactory<PinFactorEditor> weak_factory_{this};
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PIN_FACTOR_EDITOR_H_
