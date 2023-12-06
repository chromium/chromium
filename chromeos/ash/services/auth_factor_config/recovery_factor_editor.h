// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_RECOVERY_FACTOR_EDITOR_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_RECOVERY_FACTOR_EDITOR_H_

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::auth {

class AuthFactorConfig;

// The implementation of the RecoveryFactorEditor service.
class RecoveryFactorEditor : public mojom::RecoveryFactorEditor {
 public:
  explicit RecoveryFactorEditor(AuthFactorConfig*);
  RecoveryFactorEditor(const RecoveryFactorEditor&) = delete;
  RecoveryFactorEditor& operator=(const RecoveryFactorEditor&) = delete;
  ~RecoveryFactorEditor() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver);

  void Configure(const std::string& auth_token,
                 bool enabled,
                 base::OnceCallback<void(mojom::ConfigureResult)>) override;

 private:
  void OnGetEditable(const std::string& auth_token,
                     bool should_enable,
                     base::OnceCallback<void(mojom::ConfigureResult)> callback,
                     bool is_editable);
  void ConfigureWithContext(
      const std::string& auth_token,
      bool should_enable,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      bool is_editable,
      std::unique_ptr<UserContext> user_context);
  void OnRecoveryFactorConfigured(
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      const std::string& auth_token,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);

  raw_ptr<AuthFactorConfig> auth_factor_config_;
  AuthFactorEditor auth_factor_editor_;
  mojo::ReceiverSet<mojom::RecoveryFactorEditor> receivers_;
  base::WeakPtrFactory<RecoveryFactorEditor> weak_factory_{this};
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_RECOVERY_FACTOR_EDITOR_H_
