// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/sensor_hal_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"

namespace chromeos {
namespace sensors {

namespace {
SensorHalDispatcher* g_sensor_hal_dispatcher = nullptr;
}

// static
void SensorHalDispatcher::Initialize() {
  if (g_sensor_hal_dispatcher) {
    LOG(WARNING) << "SensorHalDispatcher was already initialized";
    return;
  }
  g_sensor_hal_dispatcher = new SensorHalDispatcher();
}

// static
void SensorHalDispatcher::Shutdown() {
  if (!g_sensor_hal_dispatcher) {
    LOG(WARNING)
        << "SensorHalDispatcher::Shutdown() called with null dispatcher";
    return;
  }
  delete g_sensor_hal_dispatcher;
  g_sensor_hal_dispatcher = nullptr;
}

// static
SensorHalDispatcher* SensorHalDispatcher::GetInstance() {
  return g_sensor_hal_dispatcher;
}

void SensorHalDispatcher::RegisterServer(
    mojo::PendingRemote<mojom::SensorHalServer> remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sensor_hal_server_.Bind(std::move(remote));
  sensor_hal_server_.set_disconnect_handler(
      base::BindOnce(&SensorHalDispatcher::OnSensorHalServerDisconnect,
                     base::Unretained(this)));

  // Set up the Mojo channels for clients which registered before the server
  // registers.
  for (auto& client : sensor_hal_clients_)
    EstablishMojoChannel(client);
}

void SensorHalDispatcher::RegisterClient(
    mojo::PendingRemote<mojom::SensorHalClient> remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto client = mojo::Remote<mojom::SensorHalClient>(std::move(remote));

  if (sensor_hal_server_)
    EstablishMojoChannel(client);

  sensor_hal_clients_.Add(std::move(client));
}

SensorHalDispatcher::SensorHalDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sensor_hal_clients_.set_disconnect_handler(
      base::BindRepeating(&SensorHalDispatcher::OnSensorHalClientDisconnect,
                          base::Unretained(this)));
}

SensorHalDispatcher::~SensorHalDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SensorHalDispatcher::EstablishMojoChannel(
    const mojo::Remote<mojom::SensorHalClient>& client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_hal_server_);

  mojo::PendingRemote<mojom::SensorService> service_remote;
  sensor_hal_server_->CreateChannel(
      service_remote.InitWithNewPipeAndPassReceiver());
  client->SetUpChannel(std::move(service_remote));
}

void SensorHalDispatcher::OnSensorHalServerDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Sensor HAL Server connection lost";
  sensor_hal_server_.reset();
}

void SensorHalDispatcher::OnSensorHalClientDisconnect(
    mojo::RemoteSetElementId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Sensor HAL Client connection lost: " << id;
}

}  // namespace sensors
}  // namespace chromeos
