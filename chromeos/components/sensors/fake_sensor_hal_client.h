// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_CLIENT_H_
#define CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_CLIENT_H_

#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace sensors {

class FakeSensorHalClient : public mojom::SensorHalClient {
 public:
  FakeSensorHalClient();
  FakeSensorHalClient(const FakeSensorHalClient&) = delete;
  FakeSensorHalClient& operator=(const FakeSensorHalClient&) = delete;
  ~FakeSensorHalClient() override;

  void SetUpChannel(
      mojo::PendingRemote<mojom::SensorService> sensor_service) override;

  mojo::PendingRemote<mojom::SensorHalClient> PassRemote();

  bool SensorServiceIsValid();
  void ResetSensorService();

 private:
  mojo::PendingRemote<mojom::SensorService> sensor_service_;
  mojo::Receiver<mojom::SensorHalClient> receiver_{this};
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_HAL_CLIENT_H_
