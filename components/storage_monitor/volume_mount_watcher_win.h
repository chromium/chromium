// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_
#define COMPONENTS_STORAGE_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_

#include <windows.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"

namespace storage_monitor {

class TestVolumeMountWatcherWin;

// This class watches the volume mount points and sends notifications to
// StorageMonitor about the device attach/detach events.
// This is a singleton class instantiated by StorageMonitorWin.
class VolumeMountWatcherWin {
 public:
  VolumeMountWatcherWin();

  VolumeMountWatcherWin(const VolumeMountWatcherWin&) = delete;
  VolumeMountWatcherWin& operator=(const VolumeMountWatcherWin&) = delete;

  virtual ~VolumeMountWatcherWin();

  // Returns the volume file path of the drive specified by the |drive_number|.
  // |drive_number| inputs of 0 - 25 are valid. Returns an empty file path if
  // the |drive_number| is invalid.
  static base::FilePath DriveNumberToFilePath(int drive_number);

  void Init();

  // Gets the information about the device mounted at |device_path|. On success,
  // returns true and fills in |info|.
  // Can block during startup while device info is still loading.
  bool GetDeviceInfo(const base::FilePath& device_path,
                     StorageInfo* info) const;

  // Processes DEV_BROADCAST_VOLUME messages and triggers a
  // notification if appropriate.
  void OnWindowMessage(UINT event_type, LPARAM data);

  // Processes SHCNE_MEDIAINSERTED (and REMOVED).
  void OnMediaChange(WPARAM wparam, LPARAM lparam);

  // Set the volume notifications object to be used when new
  // removable volumes are found.
  void SetNotifications(StorageMonitor::Receiver* notifications);

  void EjectDevice(
      const std::string& device_id,
      base::OnceCallback<void(StorageMonitor::EjectStatus)> callback);

 protected:
  using GetDeviceDetailsCallbackType =
      base::OnceCallback<bool(const base::FilePath&, StorageInfo*)>;

  using GetAttachedDevicesCallbackType =
      base::OnceCallback<std::vector<base::FilePath>()>;

  // Handles mass storage device attach event on UI thread.
  void HandleDeviceAttachEventOnUIThread(
      const base::FilePath& device_path,
      const StorageInfo& info);

  // Handles mass storage device detach event on UI thread.
  void HandleDeviceDetachEventOnUIThread(const std::wstring& device_location);

  // UI thread delegate to set up adding storage devices.
  void AddDevicesOnUIThread(std::vector<base::FilePath> removable_devices);

  // Runs |get_device_details_callback| for |device_path| on a worker thread.
  // |volume_watcher| points back to the VolumeMountWatcherWin that called it.
  static void RetrieveInfoForDeviceAndAdd(
      const base::FilePath& device_path,
      GetDeviceDetailsCallbackType get_device_details_callback,
      base::WeakPtr<VolumeMountWatcherWin> volume_watcher);

  // Mark that a device we started a metadata check for has completed.
  virtual void DeviceCheckComplete(const base::FilePath& device_path);

  virtual GetAttachedDevicesCallbackType GetAttachedDevicesCallback() const;
  virtual GetDeviceDetailsCallbackType GetDeviceDetailsCallback() const;

  // Used for device info calls that may take a long time.
  const scoped_refptr<base::SequencedTaskRunner> device_info_task_runner_;

 private:
  friend class TestVolumeMountWatcherWin;

  // Key: Mass storage device mount point.
  // Value: Mass storage device metadata.
  typedef std::map<base::FilePath, StorageInfo> MountPointDeviceMetadataMap;

  // Maintain a set of device attribute check calls in-flight. Only accessed
  // on the UI thread. This is to try and prevent the same device from
  // occupying our worker pool in case of windows API call hangs.
  std::set<base::FilePath> pending_device_checks_;

  // A map from device mount point to device metadata. Only accessed on the UI
  // thread.
  MountPointDeviceMetadataMap device_metadata_;

  // The notifications object to use to signal newly attached volumes. Only
  // removable devices will be notified.
  raw_ptr<StorageMonitor::Receiver> notifications_;

  base::WeakPtrFactory<VolumeMountWatcherWin> weak_factory_{this};
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_
