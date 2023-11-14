// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/factor_auth_view_factory.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

[[nodiscard]] std::unique_ptr<FactorAuthView>
FactorAuthViewFactory::CreateFactorAuthView(AshAuthFactor factor) {
  switch (factor) {
    case AshAuthFactor::kGaiaPassword:
      return CreatePasswordView();
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
  }
}

std::unique_ptr<FactorAuthView> FactorAuthViewFactory::CreatePasswordView() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ash
