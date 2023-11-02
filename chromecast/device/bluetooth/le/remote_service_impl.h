// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_service.h"

namespace chromecast {
namespace bluetooth {

class GattClientManagerImpl;
class RemoteCharacteristic;
class RemoteCharacteristicImpl;
class RemoteDeviceImpl;

class RemoteServiceImpl : public RemoteService {
 public:
  RemoteServiceImpl(const RemoteServiceImpl&) = delete;
  RemoteServiceImpl& operator=(const RemoteServiceImpl&) = delete;

  // RemoteService implementation:
  std::vector<scoped_refptr<RemoteCharacteristic>> GetCharacteristics()
      override;
  scoped_refptr<RemoteCharacteristic> GetCharacteristicByUuid(
      const bluetooth_v2_shlib::Uuid& uuid) override;
  const bluetooth_v2_shlib::Uuid& uuid() const override;
  HandleId handle() const override;
  bool primary() const override;

 private:
  friend class RemoteDeviceImpl;

  static std::map<bluetooth_v2_shlib::Uuid,
                  scoped_refptr<RemoteCharacteristicImpl>>
  CreateCharMap(RemoteDeviceImpl* remote_device,
                base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
                const bluetooth_v2_shlib::Gatt::Service& service,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // May only be constructed by RemoteDevice.
  explicit RemoteServiceImpl(
      RemoteDeviceImpl* remote_device,
      base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
      const bluetooth_v2_shlib::Gatt::Service& service,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteServiceImpl() override;

  const bluetooth_v2_shlib::Gatt::Service service_;

  const std::map<bluetooth_v2_shlib::Uuid,
                 scoped_refptr<RemoteCharacteristicImpl>>
      uuid_to_characteristic_;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_SERVICE_IMPL_H_
