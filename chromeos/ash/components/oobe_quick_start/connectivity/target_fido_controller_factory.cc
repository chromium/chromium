// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller_factory.h"

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller_impl.h"

namespace ash::quick_start {

// static
std::unique_ptr<TargetFidoController> TargetFidoControllerFactory::Create(
    const NearbyConnectionsManager* nearby_connections_manager) {
  if (test_factory_) {
    return test_factory_->CreateInstance(nearby_connections_manager);
  }

  return std::make_unique<TargetFidoControllerImpl>(nearby_connections_manager);
}

// static
void TargetFidoControllerFactory::SetFactoryForTesting(
    TargetFidoControllerFactory* test_factory) {
  test_factory_ = test_factory;
}

// static
TargetFidoControllerFactory* TargetFidoControllerFactory::test_factory_ =
    nullptr;

}  // namespace ash::quick_start