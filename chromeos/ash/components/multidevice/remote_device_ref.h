// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_REF_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_REF_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"

namespace ash {
class EasyUnlockService;
namespace multidevice_setup {
class MultiDeviceSetupImpl;
}
namespace secure_channel {
class PresenceMonitorClientImpl;
class SecureChannelClientImpl;
}  // namespace secure_channel
namespace tether {
class TetherHostFetcherImpl;
class TetherHostFetcherImplTest;
}  // namespace tether

namespace multidevice {

class ProximityAuthWebUIHandler;

// Contains metadata specific to a device associated with a user's account.
// Because this metadata contains large and expensive data types, and that data
// can become stale if a Device Sync occurs during a client application's
// lifecycle, RemoteDeviceRef is implemented using a pointer to a struct
// containing this metadata; if multiple clients want to reference the same
// device, multiple RemoteDeviceRefs can be created cheaply without duplicating
// the underlying data. Should be passed by value.
class RemoteDeviceRef {
 public:
  // Static method for truncated device ID for logs.
  static std::string TruncateDeviceIdForLogs(const std::string& full_id);

  RemoteDeviceRef(const RemoteDeviceRef& other);
  ~RemoteDeviceRef();

  const std::string& user_email() const { return remote_device_->user_email; }
  const std::string& instance_id() const { return remote_device_->instance_id; }
  const std::string& name() const { return remote_device_->name; }
  const std::string& pii_free_name() const {
    return remote_device_->pii_free_name;
  }
  const std::string& public_key() const { return remote_device_->public_key; }
  const std::string& persistent_symmetric_key() const {
    return remote_device_->persistent_symmetric_key;
  }
  int64_t last_update_time_millis() const {
    return remote_device_->last_update_time_millis;
  }
  const std::vector<BeaconSeed>& beacon_seeds() const {
    return remote_device_->beacon_seeds;
  }
  const std::string& bluetooth_public_address() const {
    return remote_device_->bluetooth_public_address;
  }

  std::string GetDeviceId() const;
  SoftwareFeatureState GetSoftwareFeatureState(
      const SoftwareFeature& software_feature) const;

  // Returns a shortened device ID for the purpose of concise logging (device
  // IDs are often so long that logs are difficult to read). Note that this
  // ID is not guaranteed to be unique, so it should only be used for log.
  std::string GetTruncatedDeviceIdForLogs() const;

  // Returns the pair of IDs used with RemoteDevices: Instance ID and device ID.
  // If either ID is missing, this string will make note of that. If a device ID
  // exists, the truncated version will be presented. This function should only
  // be used for logging.
  std::string GetInstanceIdDeviceIdForLogs() const;

  bool operator==(const RemoteDeviceRef& other) const;
  bool operator!=(const RemoteDeviceRef& other) const;
  bool operator<(const RemoteDeviceRef& other) const;

 private:
  friend class multidevice_setup::MultiDeviceSetupImpl;
  friend class secure_channel::SecureChannelClientImpl;
  friend class secure_channel::PresenceMonitorClientImpl;
  friend class RemoteDeviceCache;
  friend class RemoteDeviceRefBuilder;
  friend class RemoteDeviceRefTest;
  friend bool IsSameDevice(const RemoteDevice& remote_device,
                           RemoteDeviceRef remote_device_ref);
  friend RemoteDevice* GetMutableRemoteDevice(
      const RemoteDeviceRef& remote_device_ref);
  FRIEND_TEST_ALL_PREFIXES(RemoteDeviceRefTest, TestFields);
  FRIEND_TEST_ALL_PREFIXES(RemoteDeviceRefTest, TestCopyAndAssign);

  // TODO(crbug.com/40533499): Remove these once clients have migrated to Device
  // Sync service.
  friend class EasyUnlockService;
  friend class tether::TetherHostFetcherImplTest;
  friend class tether::TetherHostFetcherImpl;
  friend class ProximityAuthWebUIHandler;

  explicit RemoteDeviceRef(std::shared_ptr<RemoteDevice> remote_device);

  // Returns the raw RemoteDevice object. Should only be used when passing
  // RemoteDevice objects through a Mojo API, which requires that the raw type
  // is passed instead of the RemoteDeviceRef wrapper object.
  const RemoteDevice& GetRemoteDevice() const;

  std::shared_ptr<const RemoteDevice> remote_device_;
};

typedef std::vector<RemoteDeviceRef> RemoteDeviceRefList;

}  // namespace multidevice
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_REF_H_
