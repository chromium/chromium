// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_MAC_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_MAC_H_

#include <DiskArbitration/DiskArbitration.h>

#include <map>
#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/weak_ptr.h"
#include "components/storage_monitor/storage_monitor.h"

namespace storage_monitor {

class ImageCaptureDeviceManager;

// This class posts notifications to listeners when a new disk
// is attached, removed, or changed.
class StorageMonitorMac final : public StorageMonitor {
 public:
  enum UpdateType {
    UPDATE_DEVICE_ADDED,
    UPDATE_DEVICE_CHANGED,
    UPDATE_DEVICE_REMOVED,
  };

  // Should only be called by browser start up code.  Use GetInstance() instead.
  StorageMonitorMac();

  StorageMonitorMac(const StorageMonitorMac&) = delete;
  StorageMonitorMac& operator=(const StorageMonitorMac&) = delete;

  ~StorageMonitorMac() override;

  void Init() override;

  void UpdateDisk(UpdateType update_type,
                  std::string* bsd_name,
                  const StorageInfo& info);

  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override;

  void EjectDevice(const std::string& device_id,
                   base::OnceCallback<void(EjectStatus)> callback) override;

 private:
  static void DiskAppearedCallback(DADiskRef disk, void* context);
  static void DiskDisappearedCallback(DADiskRef disk, void* context);
  static void DiskDescriptionChangedCallback(DADiskRef disk,
                                             CFArrayRef keys,
                                             void* context);
  void GetDiskInfoAndUpdate(DADiskRef disk, UpdateType update_type);

  bool ShouldPostNotificationForDisk(const StorageInfo& info) const;
  bool FindDiskWithMountPoint(const base::FilePath& mount_point,
                              StorageInfo* info) const;

  base::apple::ScopedCFTypeRef<DASessionRef> session_;
  // Maps disk bsd names to disk info objects. This map tracks all mountable
  // devices on the system, though only notifications for removable devices are
  // posted.
  std::map<std::string, StorageInfo> disk_info_map_;

  int pending_disk_updates_ = 0;

  std::unique_ptr<ImageCaptureDeviceManager> image_capture_device_manager_;

  base::WeakPtrFactory<StorageMonitorMac> weak_ptr_factory_{this};
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_MAC_H_
