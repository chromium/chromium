// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_DEVICE_H_
#define CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_DEVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace sensors {

class FakeSensorDevice final : public mojom::SensorDevice {
 public:
  struct ChannelData {
    ChannelData();
    ChannelData(const ChannelData&);
    ChannelData& operator=(const ChannelData&);
    ~ChannelData();

    std::string id;
    std::map<std::string, std::string> attrs;
    int64_t sample_data;
  };

  FakeSensorDevice();
  FakeSensorDevice(const FakeSensorDevice&) = delete;
  FakeSensorDevice& operator=(const FakeSensorDevice&) = delete;
  ~FakeSensorDevice() override;

  bool is_bound();
  void Bind(mojo::PendingReceiver<mojom::SensorDevice> pending_receiver);
  void OnDeviceDisconnect();

  void SetAttribute(const std::string& attr_name,
                    const std::string& attr_value);

  void SetChannels(const std::vector<ChannelData>& channels);

  // Implementation of mojom::SensorDevice.
  void SetTimeout(uint32_t timeout) override {}
  void GetAttributes(const std::vector<std::string>& attr_names,
                     GetAttributesCallback callback) override;
  void SetFrequency(double frequency, SetFrequencyCallback callback) override;
  void StartReadingSamples(
      mojo::PendingRemote<mojom::SensorDeviceSamplesObserver> observer)
      override;
  void StopReadingSamples() override;
  void GetAllChannelIds(GetAllChannelIdsCallback callback) override;
  void SetChannelsEnabled(const std::vector<int32_t>& iio_chn_indices,
                          bool en,
                          SetChannelsEnabledCallback callback) override;
  void GetChannelsEnabled(const std::vector<int32_t>& iio_chn_indices,
                          GetChannelsEnabledCallback callback) override;
  void GetChannelsAttributes(const std::vector<int32_t>& iio_chn_indices,
                             const std::string& attr_name,
                             GetChannelsAttributesCallback callback) override;

 private:
  bool ReadyToSendSample();
  void SendSample();

  std::map<std::string, std::string> attributes_;
  base::Optional<double> frequency_;
  std::vector<ChannelData> channels_;
  std::vector<bool> channels_enabled_;

  mojo::Remote<mojom::SensorDeviceSamplesObserver> observer_;

  mojo::Receiver<mojom::SensorDevice> receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_DEVICE_H_
