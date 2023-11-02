// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_SERVICE_H_
#define CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_SERVICE_H_

#include <map>
#include <set>
#include <vector>

#include "base/sequence_checker.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace sensors {

class FakeSensorService final : public mojom::SensorService {
 public:
  FakeSensorService();
  FakeSensorService(const FakeSensorService&) = delete;
  FakeSensorService& operator=(const FakeSensorService&) = delete;
  ~FakeSensorService() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::SensorService> pending_receiver);
  void ClearReceivers();
  bool HasReceivers() const;

  void SetDevice(int32_t iio_device_id,
                 std::set<mojom::DeviceType> types,
                 std::unique_ptr<FakeSensorDevice> sensor_device);

  // Implementation of mojom::SensorService.
  void GetDeviceIds(mojom::DeviceType type,
                    GetDeviceIdsCallback callback) override;
  void GetAllDeviceIds(GetAllDeviceIdsCallback callback) override;
  void GetDevice(
      int32_t iio_device_id,
      mojo::PendingReceiver<mojom::SensorDevice> device_request) override;
  void RegisterNewDevicesObserver(
      mojo::PendingRemote<mojom::SensorServiceNewDevicesObserver> observer)
      override;

 private:
  struct DeviceData {
    DeviceData();
    DeviceData(DeviceData&&);
    DeviceData& operator=(DeviceData&&);
    ~DeviceData();

    std::set<mojom::DeviceType> types;
    std::unique_ptr<FakeSensorDevice> sensor_device;
  };

  // First is the iio_device_id, second is the device's data.
  std::map<int32_t, DeviceData> devices_;

  mojo::ReceiverSet<mojom::SensorService> receiver_set_;
  mojo::RemoteSet<mojom::SensorServiceNewDevicesObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_SERVICE_H_
