// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_COMMON_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_COMMON_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

using AuthEnginesMap =
    base::flat_map<AshAuthFactor, raw_ptr<AuthFactorEngine, DanglingUntriaged>>;

class AuthHubConnector {
 public:
  virtual ~AuthHubConnector() = default;
  virtual AuthFactorEngine* GetEngine(AshAuthFactor factor) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_COMMON_H_
