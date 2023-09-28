// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PASSWORD_FACTOR_EDITOR_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PASSWORD_FACTOR_EDITOR_H_

#include <string>
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::auth {

class AuthFactorConfig;

class PasswordFactorEditor : public mojom::PasswordFactorEditor {
 public:
  PasswordFactorEditor(AuthFactorConfig* auth_factor_config,
                       QuickUnlockStorageDelegate* storage);
  ~PasswordFactorEditor() override;

  PasswordFactorEditor(const mojom::PasswordFactorEditor&) = delete;
  PasswordFactorEditor& operator=(const PasswordFactorEditor&) = delete;

  void SetLocalPassword(
      const std::string& auth_token,
      const std::string& new_password,
      base::OnceCallback<void(mojom::ConfigureResult)> callback) override;

  void CheckLocalPasswordComplexity(
      const std::string& password,
      base::OnceCallback<void(mojom::PasswordComplexity)> callback) override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::PasswordFactorEditor> receiver);

 private:
  void SetLocalPasswordWithContext(
      const std::string& auth_token,
      const std::string& new_password,
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      std::unique_ptr<UserContext> user_context);

  void OnPasswordConfigured(
      base::OnceCallback<void(mojom::ConfigureResult)> callback,
      const std::string& auth_token,
      std::unique_ptr<UserContext> context,
      absl::optional<AuthenticationError> error);

  raw_ptr<AuthFactorConfig> auth_factor_config_;
  raw_ptr<QuickUnlockStorageDelegate> quick_unlock_storage_;
  mojo::ReceiverSet<mojom::PasswordFactorEditor> receivers_;
  AuthFactorEditor auth_factor_editor_;
  base::WeakPtrFactory<PasswordFactorEditor> weak_factory_{this};
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_PASSWORD_FACTOR_EDITOR_H_
