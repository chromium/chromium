// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_ASH_SENSOR_HAL_DISPATCHER_H_
#define CHROMEOS_COMPONENTS_SENSORS_ASH_SENSOR_HAL_DISPATCHER_H_

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace sensors {

// SensorHalDispatcher is a dispatcher that receives registrations of Sensor
// Service, including the server (IIO Service) and clients (e.g. powerd,
// simpleclient and Chrome's components). It'll then establish mojo channels
// between the server and clients.
// SensorHalDispatcher should be only used on the UI thread.
class COMPONENT_EXPORT(CHROMEOS_SENSORS) SensorHalDispatcher
    : public mojo_service_manager::mojom::ServiceObserver {
 public:
  // Creates the global SensorHalDispatcher instance.
  static void Initialize();

  // Destroys the global SensorHalDispatcher instance if it exists.
  static void Shutdown();

  // Returns a pointer to the global SensorHalDispatcher instance.
  // Initialize() should already have been called.
  static SensorHalDispatcher* GetInstance();

  // Register IIO Service's mojo remote to this dispatcher.
  void RegisterServer(mojo::PendingRemote<mojom::SensorHalServer> remote);
  // Register a sensor client's mojo remote to this dispatcher.
  void RegisterClient(mojo::PendingRemote<mojom::SensorHalClient> remote);

  // Query IioSensor service via Mojo Service Manager to establish mojo
  // channels between iioservice and sensor clients.
  // Should only be called once after CrOS Mojo Service Manager is connected.
  void TryToEstablishMojoChannelByServiceManager();

  // mojo_service_manager::mojom::ServiceObserver overrides:
  void OnServiceEvent(
      mojo_service_manager::mojom::ServiceEventPtr event) override;

 private:
  SensorHalDispatcher();
  ~SensorHalDispatcher() override;

  void QueryCallback(
      mojo_service_manager::mojom::ErrorOrServiceStatePtr result);

  void OnIioSensorServiceRegistered();
  void EstablishMojoChannel(const mojo::Remote<mojom::SensorHalClient>& client);

  void OnSensorHalServerDisconnect();
  void OnSensorHalClientDisconnect(mojo::RemoteSetElementId id);

  mojo::Remote<mojom::SensorHalServer> sensor_hal_server_;
  mojo::RemoteSet<mojom::SensorHalClient> sensor_hal_clients_;

  mojo::Receiver<mojo_service_manager::mojom::ServiceObserver> receiver_;
  bool iio_sensor_available_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_ASH_SENSOR_HAL_DISPATCHER_H_
