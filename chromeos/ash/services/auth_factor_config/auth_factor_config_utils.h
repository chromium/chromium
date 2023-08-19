// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_UTILS_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_UTILS_H_

#include "chromeos/ash/components/cryptohome/auth_factor.h"

namespace ash::auth {

bool IsGaiaPassword(const cryptohome::AuthFactor& factor);

bool IsLocalPassword(const cryptohome::AuthFactor& factor);

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_UTILS_H_
