// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_H_

#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/device/bluetooth/le/ble_types.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class RemoteDescriptor;

// A proxy for a remote characteristic on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteCharacteristic
    : public base::RefCountedThreadSafe<RemoteCharacteristic> {
 public:
  using ReadCallback =
      base::OnceCallback<void(bool, const std::vector<uint8_t>&)>;
  using StatusCallback = base::OnceCallback<void(bool)>;

  RemoteCharacteristic(const RemoteCharacteristic&) = delete;
  RemoteCharacteristic& operator=(const RemoteCharacteristic&) = delete;

  // Return a list of all descriptors.
  virtual std::vector<scoped_refptr<RemoteDescriptor>> GetDescriptors() = 0;

  // Retrieves the descriptor with |uuid|, or nullptr if it doesn't exist.
  virtual scoped_refptr<RemoteDescriptor> GetDescriptorByUuid(
      const bluetooth_v2_shlib::Uuid& uuid) = 0;

  // Register or deregister from a notification. Calls |SetNotification| and
  // writes the CCCD. For indication support, see method
  // |SetRegisterNotificationOrIndication|.
  virtual void SetRegisterNotification(bool enable, StatusCallback cb) = 0;

  // Enable notifications for this characteristic. Client must still write to
  // the CCCD seperately (or use |SetRegisterNotification| instead).
  virtual void SetNotification(bool enable, StatusCallback cb) = 0;

  // If notification is supported, then register or deregister notification.
  // If indication is supported, then register or deregister indication.
  // Note that notification has higher priority over indication.
  // Calls |SetNotification| and writes the CCCD.
  virtual void SetRegisterNotificationOrIndication(bool enable,
                                                   StatusCallback cb) = 0;

  // Read the characteristic with |auth_req|. When completed, |callback| will be
  // called.
  virtual void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                        ReadCallback callback) = 0;

  // Read the characteristic. Will retry if auth_req isn't met. When completed,
  // |callback| will be called.
  virtual void Read(ReadCallback callback) = 0;

  // Write |value| to the characteristic with |auth_req| and |write_type|. When
  // completed, |callback| will be called.
  virtual void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                         bluetooth_v2_shlib::Gatt::WriteType write_type,
                         const std::vector<uint8_t>& value,
                         StatusCallback callback) = 0;

  // Write |value| to the characteristic inferring write_type from
  // |permissions()|. Will retry if auth_req isn't met. When completed,
  // |callback| will be called.
  virtual void Write(const std::vector<uint8_t>& value,
                     StatusCallback callback) = 0;

  // Returns true if notifications are enabled.
  virtual bool NotificationEnabled() = 0;

  virtual const bluetooth_v2_shlib::Uuid& uuid() const = 0;
  virtual HandleId handle() const = 0;
  virtual bluetooth_v2_shlib::Gatt::Permissions permissions() const = 0;
  virtual bluetooth_v2_shlib::Gatt::Properties properties() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<RemoteCharacteristic>;

  RemoteCharacteristic() = default;
  virtual ~RemoteCharacteristic() = default;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_H_
