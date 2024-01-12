// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CROS_DISKS_FAKE_CROS_DISKS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CROS_DISKS_FAKE_CROS_DISKS_CLIENT_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"

namespace ash {

// A fake implementation of CrosDiskeClient. This class provides a fake behavior
// and the user of this class can raise a fake mouse events.
class COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS) FakeCrosDisksClient
    : public CrosDisksClient {
 public:
  using CustomMountPointCallback =
      base::RepeatingCallback<base::FilePath(const std::string&,
                                             const std::vector<std::string>&)>;
  FakeCrosDisksClient();

  FakeCrosDisksClient(const FakeCrosDisksClient&) = delete;
  FakeCrosDisksClient& operator=(const FakeCrosDisksClient&) = delete;

  ~FakeCrosDisksClient() override;

  // CrosDisksClient overrides
  void Init(dbus::Bus* bus) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Performs fake mounting for archive files. Instead of actually extracting
  // contents of archive files, this function creates a directory that
  // contains a dummy file.
  void Mount(const std::string& source_path,
             const std::string& source_format,
             const std::string& mount_label,
             const std::vector<std::string>& mount_options,
             MountAccessMode access_mode,
             RemountOption remount,
             chromeos::VoidDBusMethodCallback callback) override;

  // Deletes the directory created in Mount().
  void Unmount(const std::string& device_path,
               UnmountCallback callback) override;
  void EnumerateDevices(EnumerateDevicesCallback callback,
                        base::OnceClosure error_callback) override;
  void EnumerateMountEntries(EnumerateMountEntriesCallback callback,
                             base::OnceClosure error_callback) override;
  void Format(const std::string& device_path,
              const std::string& filesystem,
              const std::string& label,
              chromeos::VoidDBusMethodCallback callback) override;
  void SinglePartitionFormat(const std::string& device_path,
                             PartitionCallback callback) override;
  void Rename(const std::string& device_path,
              const std::string& volume_name,
              chromeos::VoidDBusMethodCallback callback) override;
  void GetDeviceProperties(const std::string& device_path,
                           GetDevicePropertiesCallback callback,
                           base::OnceClosure error_callback) override;

  // Used in tests to simulate signals sent by cros disks layer.
  // Calls corresponding methods of the registered observers.
  void NotifyMountCompleted(MountError error_code,
                            const std::string& source_path,
                            MountType mount_type,
                            const std::string& mount_path,
                            bool read_only = false);
  void NotifyFormatCompleted(FormatError error_code,
                             const std::string& device_path);
  void NotifyRenameCompleted(RenameError error_code,
                             const std::string& device_path);
  void NotifyMountEvent(MountEventType mount_event,
                        const std::string& device_path);

  // Add a callback to be executed when a Mount call is made to a URI
  // source_path. The mount point from the first non empty result will be used
  // in the order added.
  void AddCustomMountPointCallback(
      CustomMountPointCallback custom_mount_point_callback);

  // Returns how many times Unmount() was called.
  int unmount_call_count() const { return unmount_call_count_; }

  // Returns the |device_path| parameter from the last invocation of Unmount().
  const std::string& last_unmount_device_path() const {
    return last_unmount_device_path_;
  }

  // Makes the subsequent Unmount() calls fail. Unmount() succeeds by default.
  void MakeUnmountFail(MountError error_code) { unmount_error_ = error_code; }

  // Sets a listener callbackif the following Unmount() call is success or not.
  // Unmount() calls the corresponding callback given as a parameter.
  void set_unmount_listener(base::RepeatingClosure listener) {
    unmount_listener_ = listener;
  }

  // Returns how many times Format() was called.
  int format_call_count() const { return format_call_count_; }

  // Returns the |device_path| parameter from the last invocation of Format().
  const std::string& last_format_device_path() const {
    return last_format_device_path_;
  }

  // Returns the |filesystem| parameter from the last invocation of Format().
  const std::string& last_format_filesystem() const {
    return last_format_filesystem_;
  }

  // Returns the |label| parameter from the last invocation of Format().
  const std::string& last_format_label() const { return last_format_label_; }

  // Makes the subsequent Format() calls fail. Format() succeeds by default.
  void MakeFormatFail() { format_success_ = false; }

  // Returns how many times Format() was called.
  int partition_call_count() const { return partition_call_count_; }

  // Returns the |device_path| parameter from the last invocation of Format().
  const std::string& last_partition_device_path() const {
    return last_partition_device_path_;
  }

  // Sets the SinglePartitionFormat() result code for the callback.
  // Non error by default.
  void SetPartitionResult(PartitionError error) { partition_error_ = error; }

  // Returns how many times Rename() was called.
  int rename_call_count() const { return rename_call_count_; }

  // Returns the |device_path| parameter from the last invocation of Rename().
  const std::string& last_rename_device_path() const {
    return last_rename_device_path_;
  }

  // Returns the |volume_name| parameter from the last invocation of Rename().
  const std::string& last_rename_volume_name() const {
    return last_rename_volume_name_;
  }

  // Makes the subsequent Rename() calls fail. Rename() succeeds by default.
  void MakeRenameFail() { rename_success_ = false; }

  // Makes the subsequent GetDeviceProperties return the given |disk_info|. The
  // |disk_info| object needs to outlive all GetDeviceProperties calls.
  void set_next_get_device_properties_disk_info(const DiskInfo* disk_info) {
    next_get_device_properties_disk_info_ = disk_info;
  }

  // Returns how many times GetDeviceProperties() was called and succeeded.
  int get_device_properties_success_count() const {
    return get_device_properties_success_count_;
  }

  // Prevent subsequent Mount() calls from taking any action or responding via
  // its callback.
  void BlockMount() { block_mount_ = true; }

 private:
  // Continuation of Mount().
  void DidMount(const std::string& source_path,
                MountType type,
                const base::FilePath& mounted_path,
                chromeos::VoidDBusMethodCallback callback,
                MountError mount_error);

  base::ObserverList<Observer> observer_list_;
  int unmount_call_count_ = 0;
  std::string last_unmount_device_path_;
  MountError unmount_error_ = MountError::kSuccess;
  base::RepeatingClosure unmount_listener_;
  int format_call_count_ = 0;
  std::string last_format_device_path_;
  std::string last_format_filesystem_;
  std::string last_format_label_;
  bool format_success_ = true;
  int partition_call_count_ = 0;
  std::string last_partition_device_path_;
  PartitionError partition_error_ = PartitionError::kSuccess;
  int rename_call_count_ = 0;
  std::string last_rename_device_path_;
  std::string last_rename_volume_name_;
  bool rename_success_ = true;
  std::set<base::FilePath> mounted_paths_;
  std::vector<CustomMountPointCallback> custom_mount_point_callbacks_;
  raw_ptr<const DiskInfo> next_get_device_properties_disk_info_ = nullptr;
  int get_device_properties_success_count_ = 0;
  bool block_mount_ = false;

  base::WeakPtrFactory<FakeCrosDisksClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CROS_DISKS_FAKE_CROS_DISKS_CLIENT_H_
