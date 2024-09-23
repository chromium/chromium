// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_ENGINE_API_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_ENGINE_API_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class AuthHubConnector;

// This class is an entry point for factor UI elements to trigger
// authentication attempt in corresponding engine upon user input.
// UI components should only issue attempts when their status is reported
// as `kFactorReady`, otherwise engine might either ignore attempt it or
// queue it.
// This API allows UI elements to send data to engines without exposing
// any implementation details.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthEngineApi {
 public:
  static void AuthenticateWithPassword(AuthHubConnector* connector,
                                       AshAuthFactor factor,
                                       const std::string& raw_password);

  static void AuthenticateWithPin(AuthHubConnector* connector,
                                  AshAuthFactor factor,
                                  const std::string& raw_pin);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_ENGINE_API_H_
