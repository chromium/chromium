// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class GattClientManagerImpl;
class RemoteDeviceImpl;

class RemoteDescriptorImpl : public RemoteDescriptor {
 public:
  RemoteDescriptorImpl(const RemoteDescriptorImpl&) = delete;
  RemoteDescriptorImpl& operator=(const RemoteDescriptorImpl&) = delete;

  // RemoteDescriptor implementation:
  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback) override;
  void Read(ReadCallback callback) override;
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback) override;
  void Write(const std::vector<uint8_t>& value,
             StatusCallback callback) override;
  const bluetooth_v2_shlib::Uuid uuid() const override;
  HandleId handle() const override;
  bluetooth_v2_shlib::Gatt::Permissions permissions() const override;

  const bluetooth_v2_shlib::Gatt::Descriptor& descriptor() const;

  // Mark the object as out of scope.
  void Invalidate();

 private:
  friend class GattClientManagerImpl;
  friend class RemoteCharacteristicImpl;
  friend class RemoteDeviceImpl;

  RemoteDescriptorImpl(
      RemoteDeviceImpl* device,
      base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
      const bluetooth_v2_shlib::Gatt::Descriptor* descriptor,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteDescriptorImpl() override;

  RemoteDeviceImpl* const device_;
  base::WeakPtr<GattClientManagerImpl> gatt_client_manager_;
  const bluetooth_v2_shlib::Gatt::Descriptor* const descriptor_;

  // All bluetooth_v2_shlib calls are run on this task_runner. All members must
  // be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_IMPL_H_
