// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/volume_mounter/volume_mounter_mojom_traits.h"

#include "base/logging.h"

namespace mojo {

arc::mojom::DeviceType
EnumTraits<arc::mojom::DeviceType, chromeos::DeviceType>::ToMojom(
    chromeos::DeviceType device_type) {
  switch (device_type) {
    case chromeos::DeviceType::DEVICE_TYPE_USB:
      return arc::mojom::DeviceType::DEVICE_TYPE_USB;
    case chromeos::DeviceType::DEVICE_TYPE_SD:
      return arc::mojom::DeviceType::DEVICE_TYPE_SD;
    case chromeos::DeviceType::DEVICE_TYPE_UNKNOWN:
    case chromeos::DeviceType::DEVICE_TYPE_OPTICAL_DISC:
    case chromeos::DeviceType::DEVICE_TYPE_MOBILE:
    case chromeos::DeviceType::DEVICE_TYPE_DVD:
      // Android doesn't recognize this device natively. So, propagating
      // UNKNOWN and let Android decides how to handle this.
      return arc::mojom::DeviceType::DEVICE_TYPE_UNKNOWN;
  }
  NOTREACHED();
  return arc::mojom::DeviceType::DEVICE_TYPE_UNKNOWN;
}

bool EnumTraits<arc::mojom::DeviceType, chromeos::DeviceType>::FromMojom(
    arc::mojom::DeviceType input,
    chromeos::DeviceType* out) {
  switch (input) {
    case arc::mojom::DeviceType::DEVICE_TYPE_USB:
      *out = chromeos::DeviceType::DEVICE_TYPE_USB;
      return true;
    case arc::mojom::DeviceType::DEVICE_TYPE_SD:
      *out = chromeos::DeviceType::DEVICE_TYPE_SD;
      return true;
    case arc::mojom::DeviceType::DEVICE_TYPE_UNKNOWN:
      *out = chromeos::DeviceType::DEVICE_TYPE_UNKNOWN;
      return true;
  }
  NOTREACHED();
  return false;
}

arc::mojom::MountEvent
EnumTraits<arc::mojom::MountEvent,
           chromeos::disks::DiskMountManager::MountEvent>::
    ToMojom(chromeos::disks::DiskMountManager::MountEvent mount_event) {
  switch (mount_event) {
    case chromeos::disks::DiskMountManager::MountEvent::MOUNTING:
      return arc::mojom::MountEvent::MOUNTING;
    case chromeos::disks::DiskMountManager::MountEvent::UNMOUNTING:
      return arc::mojom::MountEvent::UNMOUNTING;
  }
  NOTREACHED();
  return arc::mojom::MountEvent::MOUNTING;
}

bool EnumTraits<arc::mojom::MountEvent,
                chromeos::disks::DiskMountManager::MountEvent>::
    FromMojom(arc::mojom::MountEvent input,
              chromeos::disks::DiskMountManager::MountEvent* out) {
  switch (input) {
    case arc::mojom::MountEvent::MOUNTING:
      *out = chromeos::disks::DiskMountManager::MountEvent::MOUNTING;
      return true;
    case arc::mojom::MountEvent::UNMOUNTING:
      *out = chromeos::disks::DiskMountManager::MountEvent::UNMOUNTING;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
