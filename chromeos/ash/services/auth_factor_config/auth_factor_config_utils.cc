// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"

#include "chromeos/ash/components/osauth/public/auth_policy_utils.h"

namespace ash::auth {

bool IsGaiaPassword(const cryptohome::AuthFactor& factor) {
  return ash::IsGaiaPassword(factor);
}

bool IsLocalPassword(const cryptohome::AuthFactor& factor) {
  return ash::IsLocalPassword(factor);
}

}  // namespace ash::auth
