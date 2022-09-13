// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/udev_util_linux.h"

#include "base/files/file_path.h"
#include "device/udev_linux/scoped_udev.h"

namespace storage_monitor {

bool GetUdevDevicePropertyValueByPath(const base::FilePath& device_path,
                                      const char* key,
                                      std::string* result) {
  device::ScopedUdevPtr udev(device::udev_new());
  if (!udev.get())
    return false;
  device::ScopedUdevDevicePtr device(device::udev_device_new_from_syspath(
      udev.get(), device_path.value().c_str()));
  if (!device.get())
    return false;
  *result = device::UdevDeviceGetPropertyValue(device.get(), key);
  return true;
}

}  // namespace storage_monitor
