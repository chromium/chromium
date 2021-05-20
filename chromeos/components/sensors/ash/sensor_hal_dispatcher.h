// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_ASH_SENSOR_HAL_DISPATCHER_H_
#define CHROMEOS_COMPONENTS_SENSORS_ASH_SENSOR_HAL_DISPATCHER_H_

#include <set>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace sensors {

// SensorHalDispatcher is a dispatcher that receives registrations of Sensor
// Service, including the server (IIO Service) and clients (e.g. powerd,
// simpleclient and Chrome's components). It'll then establish mojo channels
// between the server and clients.
// SensorHalDispatcher should be only used on the UI thread.
class COMPONENT_EXPORT(CHROMEOS_SENSORS) SensorHalDispatcher {
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

  // Get a token for a trusted client, which will be used in
  // |AuthenticateClient|.
  base::UnguessableToken GetTokenForTrustedClient();

  // Authenticate the client with |token|. If the caller get true in the
  // callback, it can further register the client's mojo channel with
  // |RegisterClient|.
  bool AuthenticateClient(const base::UnguessableToken& token);

 private:
  SensorHalDispatcher();
  ~SensorHalDispatcher();

  void EstablishMojoChannel(const mojo::Remote<mojom::SensorHalClient>& client);

  void OnSensorHalServerDisconnect();
  void OnSensorHalClientDisconnect(mojo::RemoteSetElementId id);

  mojo::Remote<mojom::SensorHalServer> sensor_hal_server_;
  mojo::RemoteSet<mojom::SensorHalClient> sensor_hal_clients_;

  std::set<base::UnguessableToken> client_token_set_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_ASH_SENSOR_HAL_DISPATCHER_H_
