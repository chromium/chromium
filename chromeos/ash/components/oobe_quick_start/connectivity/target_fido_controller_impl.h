// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_IMPL_H_

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller.h"

namespace ash::quick_start {

class NearbyConnectionsManager;

class TargetFidoControllerImpl : public TargetFidoController {
 public:
  using ResultCallback = TargetFidoController::ResultCallback;

  explicit TargetFidoControllerImpl(
      const NearbyConnectionsManager* nearby_connections_manager);
  TargetFidoControllerImpl(const TargetFidoControllerImpl&) = delete;
  TargetFidoControllerImpl& operator=(const TargetFidoControllerImpl&) = delete;
  ~TargetFidoControllerImpl() override;

  // TargetFidoController:
  void RequestAssertion(const std::string& challenge_bytes,
                        ResultCallback callback) override;

 private:
  // TODO(b/234655072): Remove maybe_unused tag after NearbyConnectionsManager
  // defined.
  [[maybe_unused]] const NearbyConnectionsManager* nearby_connections_manager_;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_IMPL_H_