// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_V2_LOADER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_V2_LOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/ash/services/device_sync/remote_device_v2_loader.h"
#include "chromeos/ash/services/device_sync/remote_device_v2_loader_impl.h"

namespace ash {

namespace device_sync {

class FakeRemoteDeviceV2Loader : public RemoteDeviceV2Loader {
 public:
  FakeRemoteDeviceV2Loader();

  // Disallow copy and assign.
  FakeRemoteDeviceV2Loader(const FakeRemoteDeviceV2Loader&) = delete;
  FakeRemoteDeviceV2Loader& operator=(const FakeRemoteDeviceV2Loader&) = delete;

  ~FakeRemoteDeviceV2Loader() override;

  // Returns the Instance ID to device map that was passed into Load(). Returns
  // null if Load() has not been called.
  const std::optional<CryptAuthDeviceRegistry::InstanceIdToDeviceMap>&
  id_to_device_map() const {
    return id_to_device_map_;
  }

  // Returns the user email that was passed into Load(). Returns null if Load()
  // has not been called.
  const std::optional<std::string>& user_email() const { return user_email_; }

  // Returns the user private key that was passed into Load(). Returns null if
  // Load() has not been called.
  const std::optional<std::string>& user_private_key() const {
    return user_private_key_;
  }

  LoadCallback& callback() { return callback_; }

 private:
  // RemoteDeviceV2Loader:
  void Load(
      const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& id_to_device_map,
      const std::string& user_email,
      const std::string& user_private_key,
      LoadCallback callback) override;

  std::optional<CryptAuthDeviceRegistry::InstanceIdToDeviceMap>
      id_to_device_map_;
  std::optional<std::string> user_email_;
  std::optional<std::string> user_private_key_;
  LoadCallback callback_;
};

class FakeRemoteDeviceV2LoaderFactory
    : public RemoteDeviceV2LoaderImpl::Factory {
 public:
  FakeRemoteDeviceV2LoaderFactory();

  // Disallow copy and assign.
  FakeRemoteDeviceV2LoaderFactory(const FakeRemoteDeviceV2LoaderFactory&) =
      delete;
  FakeRemoteDeviceV2LoaderFactory& operator=(const FakeRemoteDeviceV2Loader&) =
      delete;

  ~FakeRemoteDeviceV2LoaderFactory() override;

  // Returns a vector of all FakeRemoteDeviceV2Loader instances created by
  // CreateInstance().
  const std::vector<raw_ptr<FakeRemoteDeviceV2Loader, VectorExperimental>>&
  instances() const {
    return instances_;
  }

 private:
  // RemoteDeviceV2LoaderImpl::Factory:
  std::unique_ptr<RemoteDeviceV2Loader> CreateInstance() override;

  std::vector<raw_ptr<FakeRemoteDeviceV2Loader, VectorExperimental>> instances_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_V2_LOADER_H_
