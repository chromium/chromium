// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/fake_sensor_device.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {
namespace sensors {

FakeSensorDevice::ChannelData::ChannelData() = default;
FakeSensorDevice::ChannelData::ChannelData(
    const FakeSensorDevice::ChannelData&) = default;
FakeSensorDevice::ChannelData& FakeSensorDevice::ChannelData::operator=(
    const FakeSensorDevice::ChannelData&) = default;
FakeSensorDevice::ChannelData::~ChannelData() = default;

FakeSensorDevice::FakeSensorDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FakeSensorDevice::~FakeSensorDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FakeSensorDevice::is_bound() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return receiver_.is_bound();
}

void FakeSensorDevice::Bind(
    mojo::PendingReceiver<mojom::SensorDevice> pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_bound());

  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeSensorDevice::OnDeviceDisconnect, base::Unretained(this)));
}

void FakeSensorDevice::OnDeviceDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receiver_.reset();
}

void FakeSensorDevice::SetAttribute(const std::string& attr_name,
                                    const std::string& attr_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  attributes_[attr_name] = attr_value;
}

void FakeSensorDevice::SetChannels(const std::vector<ChannelData>& channels) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(channels_.empty());

  channels_ = channels;
  channels_enabled_.assign(channels_.size(), false);
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

  frequency_ = frequency;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(frequency)));

  if (ReadyToSendSample())
    SendSample();
}

void FakeSensorDevice::StartReadingSamples(
    mojo::PendingRemote<mojom::SensorDeviceSamplesObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (observer_.is_bound()) {
    mojo::Remote<mojom::SensorDeviceSamplesObserver> remote(
        std::move(observer));
    remote->OnErrorOccurred(mojom::ObserverErrorType::ALREADY_STARTED);

    return;
  }

  observer_.Bind(std::move(observer));
  // Reuse StopReadingSamples to reset |observer_|.
  observer_.set_disconnect_handler(base::BindOnce(
      &FakeSensorDevice::StopReadingSamples, base::Unretained(this)));

  if (!frequency_.has_value() || frequency_.value() <= 0.0) {
    observer_->OnErrorOccurred(mojom::ObserverErrorType::FREQUENCY_INVALID);
  }

  if (!std::any_of(channels_enabled_.begin(), channels_enabled_.end(),
                   [](bool en) { return en; }))
    observer_->OnErrorOccurred(mojom::ObserverErrorType::NO_ENABLED_CHANNELS);

  if (ReadyToSendSample())
    SendSample();
}

void FakeSensorDevice::StopReadingSamples() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_.reset();
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

  std::vector<int32_t> failed_indices;
  for (int32_t index : iio_chn_indices) {
    if (static_cast<size_t>(index) >= channels_enabled_.size()) {
      failed_indices.push_back(index);
      continue;
    }

    channels_enabled_[index] = en;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(failed_indices)));

  if (ReadyToSendSample())
    SendSample();
}

void FakeSensorDevice::GetChannelsEnabled(
    const std::vector<int32_t>& iio_chn_indices,
    GetChannelsEnabledCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<bool> enabled;
  for (int32_t index : iio_chn_indices) {
    if (static_cast<size_t>(index) >= channels_enabled_.size()) {
      enabled.push_back(false);
      continue;
    }

    enabled.push_back(channels_enabled_[index]);
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

bool FakeSensorDevice::ReadyToSendSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!observer_.is_bound())
    return false;

  if (!frequency_.has_value() || frequency_.value() <= 0.0)
    return false;

  return std::any_of(channels_enabled_.begin(), channels_enabled_.end(),
                     [](bool en) { return en; });
}

void FakeSensorDevice::SendSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ReadyToSendSample());
  CHECK_EQ(channels_.size(), channels_enabled_.size());

  base::flat_map<int32_t, int64_t> sample;
  for (size_t i = 0; i < channels_.size(); ++i) {
    if (!channels_enabled_[i])
      continue;

    sample[i] = channels_[i].sample_data;
  }

  observer_->OnSampleUpdated(std::move(sample));
}

}  // namespace sensors
}  // namespace chromeos
