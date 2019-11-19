// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_DISKS_CLIENT_H_
#define CHROMEOS_DBUS_CROS_DISKS_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace base {
class FilePath;
}

namespace dbus {
class Response;
}

// TODO(tbarzic): We should move these enums inside CrosDisksClient,
// to be clearer where they come from. Also, most of these are partially or
// completely duplicated in third_party/dbus/service_constants.h. We should
// probably use enums from service_contstants directly.
namespace chromeos {

// Enum describing types of mount used by cros-disks.
enum MountType {
  MOUNT_TYPE_INVALID,
  MOUNT_TYPE_DEVICE,
  MOUNT_TYPE_ARCHIVE,
  MOUNT_TYPE_NETWORK_STORAGE,
};

// Type of device.
enum DeviceType {
  DEVICE_TYPE_UNKNOWN,
  DEVICE_TYPE_USB,  // USB stick.
  DEVICE_TYPE_SD,  // SD card.
  DEVICE_TYPE_OPTICAL_DISC,  // e.g. Optical disc excluding DVD.
  DEVICE_TYPE_MOBILE,  // Storage on a mobile device (e.g. Android).
  DEVICE_TYPE_DVD,  // DVD.
};

// Mount error code used by cros-disks.
// These values are not the same as cros_disks::MountErrorType.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum MountError {
  MOUNT_ERROR_NONE,
  MOUNT_ERROR_UNKNOWN,
  MOUNT_ERROR_INTERNAL,
  MOUNT_ERROR_INVALID_ARGUMENT,
  MOUNT_ERROR_INVALID_PATH,
  MOUNT_ERROR_PATH_ALREADY_MOUNTED,
  MOUNT_ERROR_PATH_NOT_MOUNTED,
  MOUNT_ERROR_DIRECTORY_CREATION_FAILED,
  MOUNT_ERROR_INVALID_MOUNT_OPTIONS,
  MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS,
  MOUNT_ERROR_INSUFFICIENT_PERMISSIONS,
  MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND,
  MOUNT_ERROR_MOUNT_PROGRAM_FAILED,
  MOUNT_ERROR_INVALID_DEVICE_PATH,
  MOUNT_ERROR_UNKNOWN_FILESYSTEM,
  MOUNT_ERROR_UNSUPPORTED_FILESYSTEM,
  MOUNT_ERROR_INVALID_ARCHIVE,
  MOUNT_ERROR_COUNT,
};

// Rename error reported by cros-disks.
enum RenameError {
  RENAME_ERROR_NONE,
  RENAME_ERROR_UNKNOWN,
  RENAME_ERROR_INTERNAL,
  RENAME_ERROR_INVALID_DEVICE_PATH,
  RENAME_ERROR_DEVICE_BEING_RENAMED,
  RENAME_ERROR_UNSUPPORTED_FILESYSTEM,
  RENAME_ERROR_RENAME_PROGRAM_NOT_FOUND,
  RENAME_ERROR_RENAME_PROGRAM_FAILED,
  RENAME_ERROR_DEVICE_NOT_ALLOWED,
  RENAME_ERROR_LONG_NAME,
  RENAME_ERROR_INVALID_CHARACTER,
};

// Format error reported by cros-disks.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FormatError {
  FORMAT_ERROR_NONE,
  FORMAT_ERROR_UNKNOWN,
  FORMAT_ERROR_INTERNAL,
  FORMAT_ERROR_INVALID_DEVICE_PATH,
  FORMAT_ERROR_DEVICE_BEING_FORMATTED,
  FORMAT_ERROR_UNSUPPORTED_FILESYSTEM,
  FORMAT_ERROR_FORMAT_PROGRAM_NOT_FOUND,
  FORMAT_ERROR_FORMAT_PROGRAM_FAILED,
  FORMAT_ERROR_DEVICE_NOT_ALLOWED,
  FORMAT_ERROR_INVALID_OPTIONS,
  FORMAT_ERROR_LONG_NAME,
  FORMAT_ERROR_INVALID_CHARACTER,
  FORMAT_ERROR_COUNT,
};

// Event type each corresponding to a signal sent from cros-disks.
enum MountEventType {
  CROS_DISKS_DISK_ADDED,
  CROS_DISKS_DISK_REMOVED,
  CROS_DISKS_DISK_CHANGED,
  CROS_DISKS_DEVICE_ADDED,
  CROS_DISKS_DEVICE_REMOVED,
  CROS_DISKS_DEVICE_SCANNED,
};

// Mount option to control write permission to a device.
enum MountAccessMode {
  MOUNT_ACCESS_MODE_READ_WRITE,
  MOUNT_ACCESS_MODE_READ_ONLY,
};

// Whether to mount to a new path or remount a device already mounted.
enum RemountOption {
  // Mount a new device. If the device is already mounted, the mount status is
  // unchanged and the callback for MountCompleted will receive
  // MOUNT_ERROR_PATH_ALREADY_MOUNTED error code.
  REMOUNT_OPTION_MOUNT_NEW_DEVICE,
  // Remount a device that is already mounted. If the device is not mounted
  // yet, it will do nothing and the callback for MountCompleted will receive
  // MOUNT_ERROR_PATH_NOT_MOUNTED error code.
  REMOUNT_OPTION_REMOUNT_EXISTING_DEVICE,
};

// A class to represent information about a disk sent from cros-disks.
class COMPONENT_EXPORT(CHROMEOS_DBUS) DiskInfo {
 public:
  DiskInfo(const std::string& device_path, dbus::Response* response);
  ~DiskInfo();

  // Device path. (e.g. /sys/devices/pci0000:00/.../8:0:0:0/block/sdb/sdb1)
  const std::string& device_path() const { return device_path_; }

  // Disk mount path. (e.g. /media/removable/VOLUME)
  const std::string& mount_path() const { return mount_path_; }

  // Path of the scsi/mmc/nvme storage device that this disk is a part of.
  // (e.g. /sys/devices/pci0000:00/.../mmc_host/mmc0/mmc0:0002)
  const std::string& storage_device_path() const {
    return storage_device_path_;
  }

  // Is a drive or not. (i.e. true with /dev/sdb, false with /dev/sdb1)
  bool is_drive() const { return is_drive_; }

  // Does the disk have media content.
  bool has_media() const { return has_media_; }

  // Is the disk on device we booted the machine from.
  bool on_boot_device() const { return on_boot_device_; }

  // Is the disk on a removable device.
  bool on_removable_device() const { return on_removable_device_; }

  // Is the device read-only.
  bool is_read_only() const { return is_read_only_; }

  // Returns true if the device should be hidden from the file browser.
  bool is_hidden() const { return is_hidden_; }

  // Is the disk virtual.
  bool is_virtual() const { return is_virtual_; }

  // Is the disk auto-mountable.
  bool is_auto_mountable() const { return is_auto_mountable_; }

  // Disk file path (e.g. /dev/sdb).
  const std::string& file_path() const { return file_path_; }

  // Disk label.
  const std::string& label() const { return label_; }

  // Vendor ID of the device (e.g. "18d1").
  const std::string& vendor_id() const { return vendor_id_; }

  // Vendor name of the device (e.g. "Google Inc.").
  const std::string& vendor_name() const { return vendor_name_; }

  // Product ID of the device (e.g. "4e11").
  const std::string& product_id() const { return product_id_; }

  // Product name of the device (e.g. "Nexus One").
  const std::string& product_name() const { return product_name_; }

  // Disk model. (e.g. "TransMemory")
  const std::string& drive_label() const { return drive_model_; }

  // Device type. Not working well, yet.
  DeviceType device_type() const { return device_type_; }

  // Total size of the disk in bytes.
  uint64_t total_size_in_bytes() const { return total_size_in_bytes_; }

  // Returns file system uuid.
  const std::string& uuid() const { return uuid_; }

  // Returns file system type identifier.
  const std::string& file_system_type() const { return file_system_type_; }

 private:
  void InitializeFromResponse(dbus::Response* response);

  std::string device_path_;
  std::string mount_path_;
  std::string storage_device_path_;
  bool is_drive_;
  bool has_media_;
  bool on_boot_device_;
  bool on_removable_device_;
  bool is_read_only_;
  bool is_hidden_;
  bool is_virtual_;
  bool is_auto_mountable_;

  std::string file_path_;
  std::string label_;
  std::string vendor_id_;
  std::string vendor_name_;
  std::string product_id_;
  std::string product_name_;
  std::string drive_model_;
  DeviceType device_type_;
  uint64_t total_size_in_bytes_;
  std::string uuid_;
  std::string file_system_type_;
};

// A struct to represent information about a mount point sent from cros-disks.
struct COMPONENT_EXPORT(CHROMEOS_DBUS) MountEntry {
 public:
  MountEntry()
      : error_code_(MOUNT_ERROR_UNKNOWN), mount_type_(MOUNT_TYPE_INVALID) {
  }

  MountEntry(MountError error_code,
             const std::string& source_path,
             MountType mount_type,
             const std::string& mount_path)
      : error_code_(error_code),
        source_path_(source_path),
        mount_type_(mount_type),
        mount_path_(mount_path) {
  }

  MountError error_code() const { return error_code_; }
  const std::string& source_path() const { return source_path_; }
  MountType mount_type() const { return mount_type_; }
  const std::string& mount_path() const { return mount_path_; }

 private:
  MountError error_code_;
  std::string source_path_;
  MountType mount_type_;
  std::string mount_path_;
};

// A class to make the actual DBus calls for cros-disks service.
// This class only makes calls, result/error handling should be done
// by callbacks.
class COMPONENT_EXPORT(CHROMEOS_DBUS) CrosDisksClient : public DBusClient {
 public:
  // A callback to handle the result of EnumerateDevices.
  // The argument is the enumerated device paths.
  typedef base::OnceCallback<void(const std::vector<std::string>& device_paths)>
      EnumerateDevicesCallback;

  // A callback to handle the result of EnumerateMountEntries.
  // The argument is the enumerated mount entries.
  typedef base::OnceCallback<void(const std::vector<MountEntry>& entries)>
      EnumerateMountEntriesCallback;

  // A callback to handle the result of GetDeviceProperties.
  // The argument is the information about the specified device.
  typedef base::OnceCallback<void(const DiskInfo& disk_info)>
      GetDevicePropertiesCallback;

  // A callback to handle the result of Unmount.
  // The argument is the unmount error code.
  typedef base::OnceCallback<void(MountError error_code)> UnmountCallback;

  class Observer : public base::CheckedObserver {
   public:
    // Called when a mount event signal is received.
    virtual void OnMountEvent(MountEventType event_type,
                              const std::string& device_path) = 0;

    // Called when a MountCompleted signal is received.
    virtual void OnMountCompleted(const MountEntry& entry) = 0;

    // Called when a FormatCompleted signal is received.
    virtual void OnFormatCompleted(FormatError error_code,
                                   const std::string& device_path) = 0;

    // Called when a RenameCompleted signal is received.
    virtual void OnRenameCompleted(RenameError error_code,
                                   const std::string& device_path) = 0;
  };

  ~CrosDisksClient() override;

  // Registers the given |observer| to listen D-Bus signals.
  virtual void AddObserver(Observer* observer) = 0;

  // Unregisters the |observer| from this instance.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Calls Mount method.  On method call completion, |callback| is called with
  // |true| on success, or with |false| otherwise.
  // When mounting an archive, caller may set two optional arguments:
  // - The |source_format| argument passes the file extension (with the leading
  //   dot, for example ".zip"). If |source_format| is empty then the source
  //   format is auto-detected.
  // - The |mount_label| argument passes an optional mount label to be used as
  //   the directory name of the mount point. If |mount_label| is empty, the
  //   mount label will be based on the |source_path|.
  virtual void Mount(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     const std::vector<std::string>& mount_options,
                     MountAccessMode access_mode,
                     RemountOption remount,
                     VoidDBusMethodCallback callback) = 0;

  // Calls Unmount method.  On method call completion, |callback| is called
  // with the error code.
  virtual void Unmount(const std::string& device_path,
                       UnmountCallback callback) = 0;

  // Calls EnumerateDevices method.  |callback| is called after the
  // method call succeeds, otherwise, |error_callback| is called.
  virtual void EnumerateDevices(EnumerateDevicesCallback callback,
                                base::OnceClosure error_callback) = 0;

  // Calls EnumerateMountEntries.  |callback| is called after the
  // method call succeeds, otherwise, |error_callback| is called.
  virtual void EnumerateMountEntries(EnumerateMountEntriesCallback callback,
                                     base::OnceClosure error_callback) = 0;

  // Calls Format method. On completion, |callback| is called, with |true| on
  // success, or with |false| otherwise.
  virtual void Format(const std::string& device_path,
                      const std::string& filesystem,
                      const std::string& label,
                      VoidDBusMethodCallback callback) = 0;

  // Calls Rename method. On completion, |callback| is called, with |true| on
  // success, or with |false| otherwise.
  virtual void Rename(const std::string& device_path,
                      const std::string& volume_name,
                      VoidDBusMethodCallback callback) = 0;

  // Calls GetDeviceProperties method.  |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  virtual void GetDeviceProperties(const std::string& device_path,
                                   GetDevicePropertiesCallback callback,
                                   base::OnceClosure error_callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<CrosDisksClient> Create();

  // Returns the path of the mount point for archive files.
  static base::FilePath GetArchiveMountPoint();

  // Returns the path of the mount point for removable disks.
  static base::FilePath GetRemovableDiskMountPoint();

  // Composes a list of mount options.
  static std::vector<std::string> ComposeMountOptions(
      const std::vector<std::string>& options,
      const std::string& mount_label,
      MountAccessMode access_mode,
      RemountOption remount);

 protected:
  // Create() should be used instead.
  CrosDisksClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosDisksClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CROS_DISKS_CLIENT_H_
