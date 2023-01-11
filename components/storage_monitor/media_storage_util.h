// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStorageUtil provides information about storage devices attached
// to the computer.

#ifndef COMPONENTS_STORAGE_MONITOR_MEDIA_STORAGE_UTIL_H_
#define COMPONENTS_STORAGE_MONITOR_MEDIA_STORAGE_UTIL_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace storage_monitor {

class StorageInfo;

class MediaStorageUtil {
 public:
  typedef std::set<std::string /*device id*/> DeviceIdSet;

  MediaStorageUtil() = delete;
  MediaStorageUtil(const MediaStorageUtil&) = delete;
  MediaStorageUtil& operator=(const MediaStorageUtil&) = delete;

  // Check if the file system at the passed mount point looks like a media
  // device using the existence of DCIM directory.
  // Returns true if it looks like a media device, otherwise returns false.
  // Mac OS X behaves similarly, but this is not the only heuristic it uses.
  // TODO(vandebo) Try to figure out how Mac OS X decides this, and rename
  // if additional OS X heuristic is implemented.
  static bool HasDcim(const base::FilePath& mount_point);

  // Returns true if we will be able to create a filesystem for this device.
  static bool CanCreateFileSystem(const std::string& device_id,
                                  const base::FilePath& path);

  // Removes disconnected devices from |devices| and then calls |done|.
  static void FilterAttachedDevices(DeviceIdSet* devices,
                                    base::OnceClosure done);

  // Given |path|, fill in |device_info|, and |relative_path|
  // (from the root of the device).
  static bool GetDeviceInfoFromPath(const base::FilePath& path,
                                    StorageInfo* device_info,
                                    base::FilePath* relative_path);

  // Get a base::FilePath for the given |device_id|.  If the device isn't a mass
  // storage type, the base::FilePath will be empty.  This does not check that
  // the device is connected.
  static base::FilePath FindDevicePathById(const std::string& device_id);

  // Returns true if the |id| is both a removable device and also
  // currently attached.
  static bool IsRemovableStorageAttached(const std::string& id);
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_MEDIA_STORAGE_UTIL_H_
