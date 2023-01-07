// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_SENSOR_UTIL_H_
#define CHROMEOS_COMPONENTS_SENSORS_SENSOR_UTIL_H_

#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace sensors {

COMPONENT_EXPORT(CHROMEOS_SENSORS)
bool BindSensorHalClient(mojo::PendingRemote<mojom::SensorHalClient> remote);

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_SENSOR_UTIL_H_
