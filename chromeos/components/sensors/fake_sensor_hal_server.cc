// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/fake_sensor_hal_server.h"

namespace chromeos {
namespace sensors {

FakeSensorHalServer::FakeSensorHalServer()
    : sensor_service_(new FakeSensorService()) {}
FakeSensorHalServer::~FakeSensorHalServer() = default;

void FakeSensorHalServer::CreateChannel(
    mojo::PendingReceiver<mojom::SensorService> sensor_service_receiver) {
  sensor_service_->AddReceiver(std::move(sensor_service_receiver));
}

mojo::PendingRemote<mojom::SensorHalServer> FakeSensorHalServer::PassRemote() {
  DCHECK(!receiver_.is_bound());
  auto pending_remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeSensorHalServer::OnServerDisconnect, base::Unretained(this)));

  return pending_remote;
}

void FakeSensorHalServer::OnServerDisconnect() {
  receiver_.reset();
}

FakeSensorService* FakeSensorHalServer::GetSensorService() {
  return sensor_service_.get();
}

}  // namespace sensors
}  // namespace chromeos
