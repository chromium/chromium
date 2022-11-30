// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_SERVER_H_
#define CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_SERVER_H_

#include "chromeos/components/sensors/fake_sensor_service.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace sensors {

class FakeSensorHalServer : public mojom::SensorHalServer {
 public:
  FakeSensorHalServer();
  FakeSensorHalServer(const FakeSensorHalServer&) = delete;
  FakeSensorHalServer& operator=(const FakeSensorHalServer&) = delete;
  ~FakeSensorHalServer() override;

  // Implementation of mojom::SensorService.
  void CreateChannel(mojo::PendingReceiver<mojom::SensorService>
                         sensor_service_receiver) override;

  void OnServerDisconnect();

  mojo::PendingRemote<mojom::SensorHalServer> PassRemote();

  FakeSensorService* GetSensorService();

 private:
  std::unique_ptr<FakeSensorService> sensor_service_;
  mojo::Receiver<mojom::SensorHalServer> receiver_{this};
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_SERVER_H_
