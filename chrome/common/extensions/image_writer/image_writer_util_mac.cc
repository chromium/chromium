// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/image_writer/image_writer_util_mac.h"

#include <IOKit/storage/IOMedia.h>

#include "base/files/drive_info.h"

namespace extensions {

bool IsSuitableRemovableStorageDevice(io_object_t disk_obj,
                                      std::string* out_bsd_name,
                                      uint64_t* out_size_in_bytes,
                                      bool* out_removable) {
  std::optional<base::DriveInfo> info = base::GetIOObjectDriveInfo(disk_obj);
  if (!info.has_value()) {
    return false;
  }

  // Device must both be removable, and have USB. Do not allow APFS containers
  // or Core Storage volumes, even though they are marked as "whole media", as
  // they are entirely contained on a different volume.
  if (!info->is_removable.value_or(false) || !info->is_usb.value_or(false) ||
      info->is_apfs.value_or(false) || info->is_core_storage.value_or(false)) {
    return false;
  }

  if (out_size_in_bytes) {
    *out_size_in_bytes = info->size_bytes.value_or(0);
  }
  if (out_bsd_name) {
    *out_bsd_name = info->bsd_name.value_or(std::string());
  }

  if (out_removable) {
    // We already determined that removable has a value.
    *out_removable = *info->is_removable;
  }

  return true;
}

}  // namespace extensions
