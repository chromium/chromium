// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_SERVER_H_
#define CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_SERVER_H_

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

  void CreateChannel(mojo::PendingReceiver<mojom::SensorService>
                         sensor_service_receiver) override;

  mojo::PendingRemote<mojom::SensorHalServer> PassRemote();

  bool SensorServiceIsValid();
  void ResetSensorService();

 private:
  mojo::PendingReceiver<mojom::SensorService> sensor_service_receiver_;
  mojo::Receiver<mojom::SensorHalServer> receiver_{this};
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_SERVER_H_
