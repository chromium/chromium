// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_IMPL_H_

#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"

namespace chromecast {
namespace bluetooth {

class GattClientManagerImpl;
class RemoteDescriptor;
class RemoteDescriptorImpl;
class RemoteDeviceImpl;

// A proxy for a remote characteristic on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteCharacteristicImpl : public RemoteCharacteristic {
 public:
  RemoteCharacteristicImpl(const RemoteCharacteristicImpl&) = delete;
  RemoteCharacteristicImpl& operator=(const RemoteCharacteristicImpl&) = delete;

  // RemoteCharacteristic impl:
  std::vector<scoped_refptr<RemoteDescriptor>> GetDescriptors() override;
  scoped_refptr<RemoteDescriptor> GetDescriptorByUuid(
      const bluetooth_v2_shlib::Uuid& uuid) override;
  void SetRegisterNotification(bool enable, StatusCallback cb) override;
  void SetNotification(bool enable, StatusCallback cb) override;
  void SetRegisterNotificationOrIndication(bool enable,
                                           StatusCallback cb) override;
  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback) override;
  void Read(ReadCallback callback) override;
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 bluetooth_v2_shlib::Gatt::WriteType write_type,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback) override;
  void Write(const std::vector<uint8_t>& value,
             StatusCallback callback) override;
  bool NotificationEnabled() override;
  const bluetooth_v2_shlib::Uuid& uuid() const override;
  HandleId handle() const override;
  bluetooth_v2_shlib::Gatt::Permissions permissions() const override;
  bluetooth_v2_shlib::Gatt::Properties properties() const override;

  const bluetooth_v2_shlib::Gatt::Characteristic& characteristic() const;

  // Mark the object as out of scope.
  void Invalidate();

 private:
  friend class GattClientManagerImpl;
  friend class RemoteDeviceImpl;
  friend class RemoteServiceImpl;

  RemoteCharacteristicImpl(
      RemoteDeviceImpl* device,
      base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
      const bluetooth_v2_shlib::Gatt::Characteristic* characteristic,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteCharacteristicImpl() override;

  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptorImpl>>
  CreateDescriptorMap();

  // If |indication| is true, register or deregister indication.
  // If |indication| is false, register or deregister notification.
  void SetRegisterNotificationOrIndicationInternal(bool indication,
                                                   bool enable,
                                                   StatusCallback cb);

  // Weak reference to avoid refcount loop.
  RemoteDeviceImpl* const device_;
  base::WeakPtr<GattClientManagerImpl> gatt_client_manager_;
  const bluetooth_v2_shlib::Gatt::Characteristic* const characteristic_;

  // All bluetooth_v2_shlib calls are run on this task_runner. All members must
  // be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Work around http://crbug/831878. This allows notifications on
  // characteristics which do not have a CCCD.
  const std::unique_ptr<bluetooth_v2_shlib::Gatt::Descriptor> fake_cccd_;

  const std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptorImpl>>
      uuid_to_descriptor_;

  std::atomic<bool> notification_enabled_{false};
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_IMPL_H_
