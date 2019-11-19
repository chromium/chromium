// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VOLUME_MOUNTER_VOLUME_MOUNTER_MOJOM_TRAITS_H_
#define COMPONENTS_ARC_VOLUME_MOUNTER_VOLUME_MOUNTER_MOJOM_TRAITS_H_

#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/mojom/volume_mounter.mojom.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::DeviceType, chromeos::DeviceType> {
  static arc::mojom::DeviceType ToMojom(chromeos::DeviceType device_type);
  static bool FromMojom(arc::mojom::DeviceType input,
                        chromeos::DeviceType* out);
};

template <>
struct EnumTraits<arc::mojom::MountEvent,
                  chromeos::disks::DiskMountManager::MountEvent> {
  static arc::mojom::MountEvent ToMojom(
      chromeos::disks::DiskMountManager::MountEvent mount_event);
  static bool FromMojom(arc::mojom::MountEvent input,
                        chromeos::disks::DiskMountManager::MountEvent* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_VOLUME_MOUNTER_VOLUME_MOUNTER_MOJOM_TRAITS_H_
