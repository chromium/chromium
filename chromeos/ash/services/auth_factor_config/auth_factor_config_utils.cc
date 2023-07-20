// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"

namespace ash::auth {

bool IsGaiaPassword(const cryptohome::AuthFactor& factor) {
  if (factor.ref().type() != cryptohome::AuthFactorType::kPassword) {
    return false;
  }

  const std::string& label = factor.ref().label().value();
  return label == kCryptohomeGaiaKeyLabel ||
         label.find(kCryptohomeGaiaKeyLegacyLabelPrefix) == 0;
}

bool IsLocalPassword(const cryptohome::AuthFactor& factor) {
  if (factor.ref().type() != cryptohome::AuthFactorType::kPassword) {
    return false;
  }

  const std::string& label = factor.ref().label().value();
  return label == kCryptohomeLocalPasswordKeyLabel;
}

}  // namespace ash::auth
