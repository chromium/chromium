// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

using chromeos::mojo_service_manager::mojom::ErrorOrServiceState;
using chromeos::mojo_service_manager::mojom::ServiceState;

namespace chromeos {
namespace sensors {

namespace {
SensorHalDispatcher* g_sensor_hal_dispatcher = nullptr;

constexpr base::TimeDelta kRequestSensorServiceTimeout = base::Seconds(1);
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
  if (iio_sensor_available_) {
    LOG(ERROR)
        << "SensorHalServer shouldn't be used with Mojo Service Manager: "
        << mojo_services::kIioSensor << " enabled";
    return;
  }

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

  if (iio_sensor_available_ || sensor_hal_server_)
    EstablishMojoChannel(client);

  sensor_hal_clients_.Add(std::move(client));
}

SensorHalDispatcher::SensorHalDispatcher() : receiver_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sensor_hal_clients_.set_disconnect_handler(
      base::BindRepeating(&SensorHalDispatcher::OnSensorHalClientDisconnect,
                          base::Unretained(this)));
}

void SensorHalDispatcher::TryToEstablishMojoChannelByServiceManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(ash::mojo_service_manager::IsServiceManagerBound());
  DCHECK(!receiver_.is_bound());

  auto* proxy = ash::mojo_service_manager::GetServiceManagerProxy();
  proxy->AddServiceObserver(receiver_.BindNewPipeAndPassRemote());
  proxy->Query(mojo_services::kIioSensor,
               base::BindOnce(&SensorHalDispatcher::QueryCallback,
                              base::Unretained(this)));
}

void SensorHalDispatcher::OnServiceEvent(
    mojo_service_manager::mojom::ServiceEventPtr event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (event->service_name != mojo_services::kIioSensor)
    return;

  switch (event->type) {
    case mojo_service_manager::mojom::ServiceEvent::Type::kRegistered:
      OnIioSensorServiceRegistered();
      break;

    case mojo_service_manager::mojom::ServiceEvent::Type::kUnRegistered:
      iio_sensor_available_ = false;
      break;

    case mojo_service_manager::mojom::ServiceEvent::Type::kDefaultValue:
      LOG(WARNING) << "Unsupported kDefaultValue";
      break;
  }
}

SensorHalDispatcher::~SensorHalDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SensorHalDispatcher::QueryCallback(
    mojo_service_manager::mojom::ErrorOrServiceStatePtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (result->which()) {
    case ErrorOrServiceState::Tag::kState:
      switch (result->get_state()->which()) {
        case ServiceState::Tag::kRegisteredState:
          // IioSensor service is already registered by iioservice.
          OnIioSensorServiceRegistered();
          break;

        case ServiceState::Tag::kUnregisteredState:
          // IioSensor service hasn't been registered by iioservice, but the
          // policy file exists. Although we can establish mojo channels as
          // usual, and let Mojo Service Manager take care of waiting for the
          // service to be registered, we still wait for iioservice's registry,
          // to make sure sensor clients' are notified when iioservice is truly
          // available.
          //
          // After we deprecate SensorHalDispatcher, each sensor client can
          // decide the logic to request IioSensor service itself.
          iio_sensor_available_ = false;
          break;

        case ServiceState::Tag::kDefaultType:
          LOG(ERROR) << "Unsupported ServiceState::Tag::kDefaultType";
          break;
      }

      break;

    case ErrorOrServiceState::Tag::kError:
      LOG(ERROR) << "Error code: " << result->get_error()->code
                 << ", message: " << result->get_error()->message;
      break;

    case ErrorOrServiceState::Tag::kDefaultType:
      LOG(ERROR) << "Unknown type: " << result->get_default_type();
      break;
  }
}

void SensorHalDispatcher::OnIioSensorServiceRegistered() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (iio_sensor_available_)
    return;

  DCHECK(!sensor_hal_server_);
  iio_sensor_available_ = true;
  for (auto& client : sensor_hal_clients_)
    EstablishMojoChannel(client);
}

void SensorHalDispatcher::EstablishMojoChannel(
    const mojo::Remote<mojom::SensorHalClient>& client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(iio_sensor_available_ || sensor_hal_server_);

  mojo::PendingRemote<mojom::SensorService> service_remote;
  if (iio_sensor_available_) {
    DCHECK(ash::mojo_service_manager::IsServiceManagerBound());

    ash::mojo_service_manager::GetServiceManagerProxy()->Request(
        mojo_services::kIioSensor, kRequestSensorServiceTimeout,
        service_remote.InitWithNewPipeAndPassReceiver().PassPipe());
  } else {
    sensor_hal_server_->CreateChannel(
        service_remote.InitWithNewPipeAndPassReceiver());
  }
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
