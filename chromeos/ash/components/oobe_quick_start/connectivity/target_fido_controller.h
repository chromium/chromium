// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_H_

#include "base/callback.h"

namespace ash::quick_start {

// TargetFidoController initializes the FidoDeviceAuthenticator and the
// GetAssertionRequestHandler to begin the FIDO CTAP2 Assertion Flow. This class
// is also responsible for preparing the GetAssertionRequest and dispatching the
// request.
class TargetFidoController {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;

  TargetFidoController() = default;

  virtual ~TargetFidoController() = default;

  virtual void RequestAssertion(const std::string& challenge_bytes,
                                ResultCallback callback) = 0;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_H_