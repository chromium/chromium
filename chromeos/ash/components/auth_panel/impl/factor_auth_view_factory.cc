// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/impl/factor_auth_view_factory.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/impl/views/password_auth_view.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

[[nodiscard]] std::unique_ptr<FactorAuthView>
FactorAuthViewFactory::CreateFactorAuthView(
    AshAuthFactor factor,
    AuthFactorStore* store,
    AuthPanelEventDispatcher* dispatcher) {
  switch (factor) {
    case AshAuthFactor::kGaiaPassword:
      return CreatePasswordView(store, dispatcher);
    case AshAuthFactor::kCryptohomePin:
      return nullptr;
    case AshAuthFactor::kSmartCard:
      return nullptr;
    case AshAuthFactor::kSmartUnlock:
      return nullptr;
    case AshAuthFactor::kRecovery:
      return nullptr;
    case AshAuthFactor::kLegacyPin:
      return nullptr;
    case AshAuthFactor::kLegacyFingerprint:
      return nullptr;
    case AshAuthFactor::kLocalPassword:
      return nullptr;
    case AshAuthFactor::kFingerprint:
      return nullptr;
  }
}

std::unique_ptr<FactorAuthView> FactorAuthViewFactory::CreatePasswordView(
    AuthFactorStore* store,
    AuthPanelEventDispatcher* dispatcher) {
  return std::make_unique<PasswordAuthView>(dispatcher, store);
}

}  // namespace ash
