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
  explicit RecoveryFactorEditor(AuthFactorConfig*, QuickUnlockStorageDelegate*);
  RecoveryFactorEditor(const RecoveryFactorEditor&) = delete;
  RecoveryFactorEditor& operator=(const RecoveryFactorEditor&) = delete;
  ~RecoveryFactorEditor() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver);

  void Configure(const std::string& auth_token,
                 bool enabled,
                 base::OnceCallback<void(mojom::ConfigureResult)>) override;

 private:
  void OnRecoveryFactorConfigured(
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      std::unique_ptr<UserContext> context,
      absl::optional<AuthenticationError> error);

  void OnGetAuthFactorsConfiguration(
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      std::unique_ptr<UserContext> context,
      absl::optional<AuthenticationError> error);

  raw_ptr<AuthFactorConfig> auth_factor_config_;
  raw_ptr<QuickUnlockStorageDelegate> quick_unlock_storage_;
  AuthFactorEditor auth_factor_editor_;
  mojo::ReceiverSet<mojom::RecoveryFactorEditor> receivers_;
  base::WeakPtrFactory<RecoveryFactorEditor> weak_factory_{this};
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_RECOVERY_FACTOR_EDITOR_H_
