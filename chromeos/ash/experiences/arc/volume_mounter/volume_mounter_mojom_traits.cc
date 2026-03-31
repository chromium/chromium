// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/volume_mounter/volume_mounter_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

arc::mojom::DeviceType
EnumTraits<arc::mojom::DeviceType, ash::DeviceType>::ToMojom(
    ash::DeviceType device_type) {
  switch (device_type) {
    case ash::DeviceType::kUSB:
      return arc::mojom::DeviceType::DEVICE_TYPE_USB;
    case ash::DeviceType::kSD:
      return arc::mojom::DeviceType::DEVICE_TYPE_SD;
    case ash::DeviceType::kUnknown:
    case ash::DeviceType::kOpticalDisc:
    case ash::DeviceType::kMobile:
    case ash::DeviceType::kDVD:
      // Android doesn't recognize this device natively. So, propagating
      // UNKNOWN and let Android decides how to handle this.
      return arc::mojom::DeviceType::DEVICE_TYPE_UNKNOWN;
  }
  NOTREACHED();
}

std::optional<ash::DeviceType>
EnumTraits<arc::mojom::DeviceType, ash::DeviceType>::FromMojom(
    arc::mojom::DeviceType input) {
  switch (input) {
    case arc::mojom::DeviceType::DEVICE_TYPE_USB:
      return ash::DeviceType::kUSB;
    case arc::mojom::DeviceType::DEVICE_TYPE_SD:
      return ash::DeviceType::kSD;
    case arc::mojom::DeviceType::DEVICE_TYPE_UNKNOWN:
      return ash::DeviceType::kUnknown;
  }
  NOTREACHED();
}

arc::mojom::MountEvent
EnumTraits<arc::mojom::MountEvent, ash::disks::DiskMountManager::MountEvent>::
    ToMojom(ash::disks::DiskMountManager::MountEvent mount_event) {
  switch (mount_event) {
    case ash::disks::DiskMountManager::MountEvent::MOUNTING:
      return arc::mojom::MountEvent::MOUNTING;
    case ash::disks::DiskMountManager::MountEvent::UNMOUNTING:
      return arc::mojom::MountEvent::UNMOUNTING;
  }
  NOTREACHED();
}

std::optional<ash::disks::DiskMountManager::MountEvent>
EnumTraits<arc::mojom::MountEvent, ash::disks::DiskMountManager::MountEvent>::
    FromMojom(arc::mojom::MountEvent input) {
  switch (input) {
    case arc::mojom::MountEvent::MOUNTING:
      return ash::disks::DiskMountManager::MountEvent::MOUNTING;
    case arc::mojom::MountEvent::UNMOUNTING:
      return ash::disks::DiskMountManager::MountEvent::UNMOUNTING;
  }
  NOTREACHED();
}

}  // namespace mojo
