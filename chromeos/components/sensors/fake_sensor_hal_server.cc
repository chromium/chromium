// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/fake_sensor_hal_server.h"

namespace chromeos {
namespace sensors {

FakeSensorHalServer::FakeSensorHalServer() {}
FakeSensorHalServer::~FakeSensorHalServer() = default;

void FakeSensorHalServer::CreateChannel(
    mojo::PendingReceiver<mojom::SensorService> sensor_service_receiver) {
  DCHECK(!SensorServiceIsValid());
  sensor_service_receiver_ = std::move(sensor_service_receiver);
}

mojo::PendingRemote<mojom::SensorHalServer> FakeSensorHalServer::PassRemote() {
  CHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

bool FakeSensorHalServer::SensorServiceIsValid() {
  return sensor_service_receiver_.is_valid();
}

void FakeSensorHalServer::ResetSensorService() {
  sensor_service_receiver_.reset();
}

}  // namespace sensors
}  // namespace chromeos
