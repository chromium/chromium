// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_UDEV_UTIL_LINUX_H_
#define COMPONENTS_STORAGE_MONITOR_UDEV_UTIL_LINUX_H_

#include <string>

namespace base {
class FilePath;
}

namespace storage_monitor {

// Helper for udev_device_new_from_syspath()/udev_device_get_property_value()
// pair. |device_path| is the absolute path to the device, including /sys.
bool GetUdevDevicePropertyValueByPath(const base::FilePath& device_path,
                                      const char* key,
                                      std::string* result);

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_UDEV_UTIL_LINUX_H_
