// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chromecast/device/bluetooth/le/ble_types.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class RemoteDevice;
class RemoteDescriptor;

// A proxy for a remote descriptor on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteDescriptor : public base::RefCountedThreadSafe<RemoteDescriptor> {
 public:
  static constexpr uint8_t kEnableNotificationValue[] = {0x01, 0x00};
  static constexpr uint8_t kEnableIndicationValue[] = {0x02, 0x00};
  static constexpr uint8_t kDisableNotificationValue[] = {0x00, 0x00};
  static const bluetooth_v2_shlib::Uuid kCccdUuid;

  using ReadCallback =
      base::OnceCallback<void(bool, const std::vector<uint8_t>&)>;
  using StatusCallback = base::OnceCallback<void(bool)>;

  RemoteDescriptor(const RemoteDescriptor&) = delete;
  RemoteDescriptor& operator=(const RemoteDescriptor&) = delete;

  // Read the descriptor with |auth_req|. When completed, |callback| will be
  // called.
  virtual void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                        ReadCallback callback) = 0;

  // Read the descriptor. When completed, |callback| will be called.
  virtual void Read(ReadCallback callback) = 0;

  // Write |value| to the descriptor with |auth_req|. When completed, |callback|
  // will be called.
  virtual void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                         const std::vector<uint8_t>& value,
                         StatusCallback callback) = 0;

  // Write |value| to the descriptor. Will retry if auth_req isn't met. When
  // completed, |callback| will be called.
  virtual void Write(const std::vector<uint8_t>& value,
                     StatusCallback callback) = 0;

  virtual const bluetooth_v2_shlib::Uuid uuid() const = 0;
  virtual HandleId handle() const = 0;
  virtual bluetooth_v2_shlib::Gatt::Permissions permissions() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<RemoteDescriptor>;

  RemoteDescriptor() = default;
  virtual ~RemoteDescriptor() = default;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DESCRIPTOR_H_
