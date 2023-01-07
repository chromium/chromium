// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"
#include "ash/constants/ash_features.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"

namespace ash::auth {

RecoveryFactorEditor::RecoveryFactorEditor(AuthFactorConfig* auth_factor_config)
    : auth_factor_config_(auth_factor_config) {}
RecoveryFactorEditor::~RecoveryFactorEditor() = default;

void RecoveryFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void RecoveryFactorEditor::Configure(
    const std::string& auth_token,
    bool enabled,
    base::OnceCallback<void(ConfigureResult)> callback) {
  if (!features::IsCryptohomeRecoverySetupEnabled()) {
    // Log always, crash on debug builds.
    LOG(ERROR) << "AuthFactorConfig::Configure is a fake";
    NOTIMPLEMENTED();
    std::move(callback).Run(ConfigureResult::kClientError);
    return;
  }

  auth_factor_config_->recovery_configured_ = enabled;
  std::move(callback).Run(ConfigureResult::kSuccess);
  auth_factor_config_->NotifyFactorObservers(mojom::AuthFactor::kRecovery);
}

}  // namespace ash::auth
