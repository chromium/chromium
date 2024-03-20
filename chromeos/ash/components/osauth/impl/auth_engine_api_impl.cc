// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/public/auth_engine_api.h"

#include <string>

#include "base/check_op.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_password_engine.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_pin_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// static
void AuthEngineApi::AuthenticateWithPassword(AuthHubConnector* connector,
                                             AshAuthFactor factor,
                                             const std::string& raw_password) {
  CHECK_EQ(factor, AshAuthFactor::kGaiaPassword);
  CryptohomePasswordEngine* engine =
      static_cast<CryptohomePasswordEngine*>(connector->GetEngine(factor));
  engine->PerformPasswordAttempt(raw_password);
}

void AuthEngineApi::AuthenticateWithPin(AuthHubConnector* connector,
                                        AshAuthFactor factor,
                                        const std::string& raw_pin) {
  CHECK_EQ(factor, AshAuthFactor::kCryptohomePin);
  CryptohomePinEngine* engine =
      static_cast<CryptohomePinEngine*>(connector->GetEngine(factor));
  engine->PerformPinAttempt(raw_pin);
}

}  // namespace ash
