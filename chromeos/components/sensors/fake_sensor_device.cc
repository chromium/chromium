// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/fake_sensor_device.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {
namespace sensors {

FakeSensorDevice::ChannelData::ChannelData() = default;
FakeSensorDevice::ChannelData::ChannelData(
    const FakeSensorDevice::ChannelData&) = default;
FakeSensorDevice::ChannelData& FakeSensorDevice::ChannelData::operator=(
    const FakeSensorDevice::ChannelData&) = default;
FakeSensorDevice::ChannelData::~ChannelData() = default;

FakeSensorDevice::FakeSensorDevice(const std::vector<ChannelData>& channels)
    : channels_(channels) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& client : clients_)
    client.second.channels_enabled.assign(channels_.size(), false);
}

FakeSensorDevice::~FakeSensorDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojo::ReceiverId FakeSensorDevice::AddReceiver(
    mojo::PendingReceiver<mojom::SensorDevice> pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto id = receiver_set_.Add(this, std::move(pending_receiver));
  DCHECK(clients_.find(id) == clients_.end());
  clients_[id].channels_enabled.assign(channels_.size(), false);

  return id;
}

void FakeSensorDevice::RemoveReceiver(mojo::ReceiverId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(receiver_set_.HasReceiver(id));

  clients_.erase(id);
  receiver_set_.Remove(id);
}

void FakeSensorDevice::ClearReceivers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  clients_.clear();
  receiver_set_.Clear();
}

bool FakeSensorDevice::HasReceivers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return !receiver_set_.empty();
}

size_t FakeSensorDevice::SizeOfReceivers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return receiver_set_.size();
}

void FakeSensorDevice::SetAttribute(const std::string& attr_name,
                                    const std::string& attr_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  attributes_[attr_name] = attr_value;
}

void FakeSensorDevice::ResetObserverRemote(mojo::ReceiverId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = clients_.find(id);
  if (it == clients_.end())
    return;

  it->second.observer.reset();
}

void FakeSensorDevice::GetAttributes(const std::vector<std::string>& attr_names,
                                     GetAttributesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<base::Optional<std::string>> values;
  values.reserve(attr_names.size());
  for (const auto& attr_name : attr_names) {
    auto it = attributes_.find(attr_name);
    if (it != attributes_.end())
      values.push_back(it->second);
    else
      values.push_back(base::nullopt);
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(values)));
}

void FakeSensorDevice::SetFrequency(double frequency,
                                    SetFrequencyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (frequency < 0.0)
    frequency = 0.0;

  auto& client = clients_[receiver_set_.current_receiver()];

  client.frequency = frequency;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(frequency)));

  SendSampleIfReady(client);
}

void FakeSensorDevice::StartReadingSamples(
    mojo::PendingRemote<mojom::SensorDeviceSamplesObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto id = receiver_set_.current_receiver();
  auto& client = clients_[id];

  if (client.observer.is_bound()) {
    mojo::Remote<mojom::SensorDeviceSamplesObserver> remote(
        std::move(observer));
    remote->OnErrorOccurred(mojom::ObserverErrorType::ALREADY_STARTED);

    return;
  }

  client.observer.Bind(std::move(observer));
  client.observer.set_disconnect_handler(base::BindOnce(
      &FakeSensorDevice::ResetObserverRemote, base::Unretained(this), id));

  if (!client.frequency.has_value() || client.frequency.value() <= 0.0) {
    client.observer->OnErrorOccurred(
        mojom::ObserverErrorType::FREQUENCY_INVALID);
    return;
  }

  if (base::ranges::none_of(client.channels_enabled,
                            [](bool enabled) { return enabled; })) {
    client.observer->OnErrorOccurred(
        mojom::ObserverErrorType::NO_ENABLED_CHANNELS);
    return;
  }

  SendSampleIfReady(client);
}

void FakeSensorDevice::StopReadingSamples() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  clients_[receiver_set_.current_receiver()].observer.reset();
}

void FakeSensorDevice::GetAllChannelIds(GetAllChannelIdsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::string> channel_ids;
  for (const ChannelData& channel : channels_)
    channel_ids.push_back(channel.id);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(channel_ids)));
}

void FakeSensorDevice::SetChannelsEnabled(
    const std::vector<int32_t>& iio_chn_indices,
    bool en,
    SetChannelsEnabledCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& client = clients_[receiver_set_.current_receiver()];

  std::vector<int32_t> failed_indices;
  for (int32_t index : iio_chn_indices) {
    if (static_cast<size_t>(index) >= client.channels_enabled.size()) {
      failed_indices.push_back(index);
      continue;
    }

    client.channels_enabled[index] = en;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(failed_indices)));

  SendSampleIfReady(client);
}

void FakeSensorDevice::GetChannelsEnabled(
    const std::vector<int32_t>& iio_chn_indices,
    GetChannelsEnabledCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& client = clients_[receiver_set_.current_receiver()];

  std::vector<bool> enabled;
  for (int32_t index : iio_chn_indices) {
    if (static_cast<size_t>(index) >= client.channels_enabled.size()) {
      enabled.push_back(false);
      continue;
    }

    enabled.push_back(client.channels_enabled[index]);
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(enabled)));
}

void FakeSensorDevice::GetChannelsAttributes(
    const std::vector<int32_t>& iio_chn_indices,
    const std::string& attr_name,
    GetChannelsAttributesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<base::Optional<std::string>> attrs;

  for (const ChannelData& channel : channels_) {
    auto it = channel.attrs.find(attr_name);
    if (it == channel.attrs.end()) {
      attrs.push_back(base::nullopt);
      continue;
    }

    attrs.push_back(it->second);
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(attrs)));
}

FakeSensorDevice::ClientData::ClientData() = default;
FakeSensorDevice::ClientData::~ClientData() = default;

void FakeSensorDevice::SendSampleIfReady(ClientData& client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(channels_.size(), client.channels_enabled.size());

  if (!client.observer.is_bound())
    return;

  if (!client.frequency.has_value() || client.frequency.value() <= 0.0)
    return;

  base::flat_map<int32_t, int64_t> sample;
  for (size_t i = 0; i < channels_.size(); ++i) {
    if (!client.channels_enabled[i])
      continue;

    sample[i] = channels_[i].sample_data;
  }

  if (sample.empty())
    return;

  client.observer->OnSampleUpdated(std::move(sample));
}

}  // namespace sensors
}  // namespace chromeos
