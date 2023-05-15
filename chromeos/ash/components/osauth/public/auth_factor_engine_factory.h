// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_ENGINE_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_ENGINE_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class AuthFactorEngine;

// Class that creates configured AuthFactorEngines for AuthHub.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthFactorEngineFactory {
 public:
  virtual ~AuthFactorEngineFactory() = default;

  // AuthFactory type that created engine would support.
  virtual AshAuthFactor GetFactor() = 0;

  // Create an Engine for `GetFactor()`, configured to work in given `mode`.
  // Can return `null` if the factor is not supported in `mode` (e.g.
  // SmartLock on login screen / recovery in the session).
  // Should not be called with `mode == kNone`.
  virtual std::unique_ptr<AuthFactorEngine> CreateEngine(AuthHubMode mode) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_ENGINE_FACTORY_H_
