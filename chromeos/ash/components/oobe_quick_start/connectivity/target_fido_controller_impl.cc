// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller_impl.h"

namespace ash::quick_start {

TargetFidoControllerImpl::TargetFidoControllerImpl(
    const NearbyConnectionsManager* nearby_connections_manager)
    : nearby_connections_manager_(nearby_connections_manager) {
  // TODO(jasonrhee@): Uncomment the following line after
  // NearbyConnectionsManager defined.

  // CHECK(nearby_connections_manager_);
}

TargetFidoControllerImpl::~TargetFidoControllerImpl() = default;

void TargetFidoControllerImpl::RequestAssertion(
    const std::string& challenge_bytes,
    ResultCallback callback) {
  // TODO(b/234655072): This is a stub and not real logic. Add the actual logic.
  std::move(callback).Run(/*success=*/true);
}

}  // namespace ash::quick_start