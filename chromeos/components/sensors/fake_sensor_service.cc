// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/fake_sensor_service.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {
namespace sensors {

FakeSensorService::FakeSensorService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FakeSensorService::~FakeSensorService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FakeSensorService::is_bound() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return receiver_.is_bound();
}

void FakeSensorService::Bind(
    mojo::PendingReceiver<mojom::SensorService> pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_bound());

  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeSensorService::OnServiceDisconnect, base::Unretained(this)));
}

void FakeSensorService::OnServiceDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receiver_.reset();
}

void FakeSensorService::SetDevice(
    int32_t iio_device_id,
    std::set<mojom::DeviceType> types,
    std::unique_ptr<FakeSensorDevice> sensor_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeviceData data;
  data.types = types;
  data.sensor_device = std::move(sensor_device);
  devices_[iio_device_id] = std::move(data);

  for (auto& observer : observers_) {
    observer->OnNewDeviceAdded(iio_device_id, std::vector<mojom::DeviceType>(
                                                  types.begin(), types.end()));
  }
}

void FakeSensorService::GetDeviceIds(mojom::DeviceType type,
                                     GetDeviceIdsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<int32_t> ids;
  for (const auto& device : devices_) {
    if (device.second.types.count(type) == 0)
      continue;

    ids.push_back(device.first);
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(ids)));
}

void FakeSensorService::GetAllDeviceIds(GetAllDeviceIdsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_map<int32_t, std::vector<mojom::DeviceType>> id_types;
  for (const auto& device : devices_) {
    id_types.emplace(device.first,
                     std::vector<mojom::DeviceType>(device.second.types.begin(),
                                                    device.second.types.end()));
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(id_types)));
}

void FakeSensorService::GetDevice(
    int32_t iio_device_id,
    mojo::PendingReceiver<mojom::SensorDevice> device_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = devices_.find(iio_device_id);
  if (it == devices_.end())
    return;

  it->second.sensor_device->AddReceiver(std::move(device_request));
}

void FakeSensorService::RegisterNewDevicesObserver(
    mojo::PendingRemote<mojom::SensorServiceNewDevicesObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.Add(std::move(observer));
}

FakeSensorService::DeviceData::DeviceData() = default;
FakeSensorService::DeviceData::DeviceData(FakeSensorService::DeviceData&&) =
    default;
FakeSensorService::DeviceData& FakeSensorService::DeviceData::operator=(
    FakeSensorService::DeviceData&&) = default;
FakeSensorService::DeviceData::~DeviceData() = default;

}  // namespace sensors
}  // namespace chromeos
