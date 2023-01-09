// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/remote_service_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_characteristic_impl.h"

namespace chromecast {
namespace bluetooth {

// static
std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteCharacteristicImpl>>
RemoteServiceImpl::CreateCharMap(
    RemoteDeviceImpl* remote_device,
    base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
    const bluetooth_v2_shlib::Gatt::Service& service,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteCharacteristicImpl>>
      ret;
  for (const auto& characteristic : service.characteristics) {
    ret[characteristic.uuid] = new RemoteCharacteristicImpl(
        remote_device, gatt_client_manager, &characteristic, io_task_runner);
  }
  return ret;
}

RemoteServiceImpl::RemoteServiceImpl(
    RemoteDeviceImpl* remote_device,
    base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
    const bluetooth_v2_shlib::Gatt::Service& service,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : service_(service),
      uuid_to_characteristic_(CreateCharMap(remote_device,
                                            gatt_client_manager,
                                            service_,
                                            io_task_runner)) {
  DCHECK(gatt_client_manager);
  DCHECK(io_task_runner);
  DCHECK(io_task_runner->BelongsToCurrentThread());
}

RemoteServiceImpl::~RemoteServiceImpl() = default;

std::vector<scoped_refptr<RemoteCharacteristic>>
RemoteServiceImpl::GetCharacteristics() {
  std::vector<scoped_refptr<RemoteCharacteristic>> ret;
  ret.reserve(uuid_to_characteristic_.size());
  for (const auto& pair : uuid_to_characteristic_) {
    ret.push_back(pair.second);
  }

  return ret;
}

scoped_refptr<RemoteCharacteristic> RemoteServiceImpl::GetCharacteristicByUuid(
    const bluetooth_v2_shlib::Uuid& uuid) {
  auto it = uuid_to_characteristic_.find(uuid);
  if (it == uuid_to_characteristic_.end())
    return nullptr;
  return it->second;
}

const bluetooth_v2_shlib::Uuid& RemoteServiceImpl::uuid() const {
  return service_.uuid;
}
HandleId RemoteServiceImpl::handle() const {
  return service_.handle;
}
bool RemoteServiceImpl::primary() const {
  return service_.primary;
}

}  // namespace bluetooth
}  // namespace chromecast
