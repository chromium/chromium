// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_FACTORY_H_

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller.h"

namespace ash::quick_start {

class NearbyConnectionsManager;

class TargetFidoControllerFactory {
 public:
  static std::unique_ptr<TargetFidoController> Create(
      const NearbyConnectionsManager* nearby_connections_manager);

  static void SetFactoryForTesting(TargetFidoControllerFactory* test_factory);

 protected:
  virtual ~TargetFidoControllerFactory() = default;
  virtual std::unique_ptr<TargetFidoController> CreateInstance(
      const NearbyConnectionsManager* nearby_connections_manager) = 0;

 private:
  static TargetFidoControllerFactory* test_factory_;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_FACTORY_H_
