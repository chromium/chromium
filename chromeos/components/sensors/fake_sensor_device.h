// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_DEVICE_H_
#define CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_DEVICE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
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

  explicit FakeSensorDevice(const std::vector<ChannelData>& channels);
  FakeSensorDevice(const FakeSensorDevice&) = delete;
  FakeSensorDevice& operator=(const FakeSensorDevice&) = delete;
  ~FakeSensorDevice() override;

  mojo::ReceiverId AddReceiver(
      mojo::PendingReceiver<mojom::SensorDevice> pending_receiver);
  void RemoveReceiver(mojo::ReceiverId id);
  void RemoveReceiverWithReason(mojo::ReceiverId id,
                                mojom::SensorDeviceDisconnectReason reason,
                                const std::string& description);

  void ClearReceivers();
  void ClearReceiversWithReason(mojom::SensorDeviceDisconnectReason reason,
                                const std::string& description);
  bool HasReceivers() const;
  size_t SizeOfReceivers() const;

  void SetAttribute(const std::string& attr_name,
                    const std::string& attr_value);

  void ResetObserverRemote(mojo::ReceiverId id);
  void ResetObserverRemoteWithReason(mojo::ReceiverId id,
                                     mojom::SensorDeviceDisconnectReason reason,
                                     const std::string& description);

  // Unlike SetChannelsEnabled() below, SetChannelsEnabledWithId() is used
  // without a mojo pipe, instead, with the mojo::ReceiverId from AddReceiver().
  // It lets the tests manually enable or disable channels to simulate some
  // unexpected behaviors of iioservice, such as channels being unavailable
  // suddenly.
  void SetChannelsEnabledWithId(mojo::ReceiverId id,
                                const std::vector<int32_t>& iio_chn_indices,
                                bool en);

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
  struct ClientData {
    ClientData();
    ~ClientData();

    std::optional<double> frequency;
    std::vector<bool> channels_enabled;
    mojo::Remote<mojom::SensorDeviceSamplesObserver> observer;
  };

  void SendSampleIfReady(ClientData& client);

  std::map<std::string, std::string> attributes_;
  const std::vector<ChannelData> channels_;

  mojo::ReceiverSet<mojom::SensorDevice> receiver_set_;

  // First is the client's id from |receiver_set_|, second is the client's
  // states and observer remote.
  std::map<mojo::ReceiverId, ClientData> clients_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sensors
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SENSORS_FAKE_SENSOR_DEVICE_H_
