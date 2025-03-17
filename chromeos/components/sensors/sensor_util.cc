// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/sensor_util.h"

#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"

namespace chromeos {
namespace sensors {

bool BindSensorHalClient(mojo::PendingRemote<mojom::SensorHalClient> remote) {
  auto* dispatcher = SensorHalDispatcher::GetInstance();
  if (!dispatcher) {
    // In some unit tests it's not initialized.
    return false;
  }

  dispatcher->RegisterClient(std::move(remote));
  return true;
}

}  // namespace sensors
}  // namespace chromeos
