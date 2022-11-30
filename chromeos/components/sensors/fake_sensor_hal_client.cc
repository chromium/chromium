// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/fake_sensor_hal_client.h"

namespace chromeos {
namespace sensors {

FakeSensorHalClient::FakeSensorHalClient() {}
FakeSensorHalClient::~FakeSensorHalClient() = default;

void FakeSensorHalClient::SetUpChannel(
    mojo::PendingRemote<mojom::SensorService> sensor_service) {
  DCHECK(!SensorServiceIsValid());
  sensor_service_ = std::move(sensor_service);
}

mojo::PendingRemote<mojom::SensorHalClient> FakeSensorHalClient::PassRemote() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

bool FakeSensorHalClient::SensorServiceIsValid() {
  return sensor_service_.is_valid();
}

void FakeSensorHalClient::ResetSensorService() {
  sensor_service_.reset();
}

}  // namespace sensors
}  // namespace chromeos
