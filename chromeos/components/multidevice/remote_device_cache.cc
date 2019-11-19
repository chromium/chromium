// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/remote_device_cache.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"

namespace chromeos {

namespace multidevice {

// static
RemoteDeviceCache::Factory* RemoteDeviceCache::Factory::test_factory_ = nullptr;

// static
RemoteDeviceCache::Factory* RemoteDeviceCache::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void RemoteDeviceCache::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

RemoteDeviceCache::Factory::~Factory() = default;

std::unique_ptr<RemoteDeviceCache> RemoteDeviceCache::Factory::BuildInstance() {
  return base::WrapUnique(new RemoteDeviceCache());
}

RemoteDeviceCache::RemoteDeviceCache() = default;

RemoteDeviceCache::~RemoteDeviceCache() = default;

void RemoteDeviceCache::SetRemoteDevices(
    const RemoteDeviceList& remote_devices) {
  for (const auto& remote_device : remote_devices) {
    if (base::Contains(remote_device_map_, remote_device.GetDeviceId())) {
      // TODO(khorimoto): Do not overwrite device metadata if the new device
      // contains a stale timestamp; see https://crbug.com/856746.

      // Keep the same shared_ptr object, and simply
      // update the RemoteDevice it references. This transparently updates
      // the RemoteDeviceRefs used by clients.
      *remote_device_map_[remote_device.GetDeviceId()] = remote_device;
    } else {
      remote_device_map_[remote_device.GetDeviceId()] =
          std::make_shared<RemoteDevice>(remote_device);
    }
  }

  // Intentionally leave behind devices in the map which weren't in
  // |remote_devices|, to prevent clients from segfaulting by accessing "stale"
  // devices.
}

RemoteDeviceRefList RemoteDeviceCache::GetRemoteDevices() const {
  RemoteDeviceRefList remote_devices;
  for (const auto& it : remote_device_map_)
    remote_devices.push_back(RemoteDeviceRef(it.second));

  return remote_devices;
}

base::Optional<RemoteDeviceRef> RemoteDeviceCache::GetRemoteDevice(
    const std::string& device_id) const {
  if (!base::Contains(remote_device_map_, device_id))
    return base::nullopt;

  return RemoteDeviceRef(remote_device_map_.at(device_id));
}

}  // namespace multidevice

}  // namespace chromeos
