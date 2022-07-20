// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CROS_DISKS_CROS_DISKS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CROS_DISKS_CROS_DISKS_CLIENT_H_

#include <cstdint>
#include <ostream>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

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
  DEVICE_TYPE_USB,           // USB stick.
  DEVICE_TYPE_SD,            // SD card.
  DEVICE_TYPE_OPTICAL_DISC,  // e.g. Optical disc excluding DVD.
  DEVICE_TYPE_MOBILE,        // Storage on a mobile device (e.g. Android).
  DEVICE_TYPE_DVD,           // DVD.
};

// Mount error code used by cros-disks.
// These values are NOT the same as cros_disks::MountErrorType.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum MountError {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_UNKNOWN = 1,
  MOUNT_ERROR_INTERNAL = 2,
  MOUNT_ERROR_INVALID_ARGUMENT = 3,
  MOUNT_ERROR_INVALID_PATH = 4,
  MOUNT_ERROR_PATH_ALREADY_MOUNTED = 5,
  MOUNT_ERROR_PATH_NOT_MOUNTED = 6,
  MOUNT_ERROR_DIRECTORY_CREATION_FAILED = 7,
  MOUNT_ERROR_INVALID_MOUNT_OPTIONS = 8,
  MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS = 9,
  MOUNT_ERROR_INSUFFICIENT_PERMISSIONS = 10,
  MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND = 11,
  MOUNT_ERROR_MOUNT_PROGRAM_FAILED = 12,
  MOUNT_ERROR_INVALID_DEVICE_PATH = 13,
  MOUNT_ERROR_UNKNOWN_FILESYSTEM = 14,
  MOUNT_ERROR_UNSUPPORTED_FILESYSTEM = 15,
  MOUNT_ERROR_INVALID_ARCHIVE = 16,
  MOUNT_ERROR_NEED_PASSWORD = 17,
  MOUNT_ERROR_IN_PROGRESS = 18,
  MOUNT_ERROR_CANCELLED = 19,
  MOUNT_ERROR_COUNT,
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, MountError error);

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

// Partition error reported by cros-disks.
enum PartitionError {
  PARTITION_ERROR_NONE = 0,
  PARTITION_ERROR_UNKNOWN = 1,
  PARTITION_ERROR_INTERNAL = 2,
  PARTITION_ERROR_INVALID_DEVICE_PATH = 3,
  PARTITION_ERROR_DEVICE_BEING_PARTITIONED = 4,
  PARTITION_ERROR_PROGRAM_NOT_FOUND = 5,
  PARTITION_ERROR_PROGRAM_FAILED = 6,
  PARTITION_ERROR_DEVICE_NOT_ALLOWED = 7,
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
class COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS) DiskInfo {
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

  // USB bus number of the device (e.g. 1).
  int bus_number() const { return bus_number_; }

  // USB device number of the device (e.g. 5).
  int device_number() const { return device_number_; }

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
  int bus_number_;
  int device_number_;
  uint64_t total_size_in_bytes_;
  std::string uuid_;
  std::string file_system_type_;
};

// A struct to represent information about a mount point sent from cros-disks.
struct COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS) MountEntry {
 public:
  MountEntry()
      : error_code_(MOUNT_ERROR_UNKNOWN), mount_type_(MOUNT_TYPE_INVALID) {}

  MountEntry(MountError error_code,
             const std::string& source_path,
             MountType mount_type,
             const std::string& mount_path)
      : error_code_(error_code),
        source_path_(source_path),
        mount_type_(mount_type),
        mount_path_(mount_path) {}

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
class COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS) CrosDisksClient
    : public DBusClient {
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

  // A callback to handle the result of SinglePartitionFormat.
  // The argument is the partition error code.
  using PartitionCallback = base::OnceCallback<void(PartitionError error_code)>;

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

  // Returns the global instance if initialized. May return null.
  static CrosDisksClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  CrosDisksClient(const CrosDisksClient&) = delete;
  CrosDisksClient& operator=(const CrosDisksClient&) = delete;

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

  // Calls SinglePartitionFormat async method. |callback| is called when
  // response received.
  virtual void SinglePartitionFormat(const std::string& device_path,
                                     PartitionCallback callback) = 0;

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
  // Initialize() should be used instead.
  CrosDisksClient();
  ~CrosDisksClient() override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when //chromeos/dbus moved to ash.
namespace ash {
using ::chromeos::CROS_DISKS_DEVICE_ADDED;
using ::chromeos::CROS_DISKS_DEVICE_REMOVED;
using ::chromeos::CROS_DISKS_DEVICE_SCANNED;
using ::chromeos::CROS_DISKS_DISK_ADDED;
using ::chromeos::CROS_DISKS_DISK_REMOVED;
using ::chromeos::CrosDisksClient;
using ::chromeos::DEVICE_TYPE_MOBILE;
using ::chromeos::DEVICE_TYPE_OPTICAL_DISC;
using ::chromeos::DEVICE_TYPE_SD;
using ::chromeos::DEVICE_TYPE_UNKNOWN;
using ::chromeos::DEVICE_TYPE_USB;
using ::chromeos::DeviceType;
using ::chromeos::DiskInfo;
using ::chromeos::FORMAT_ERROR_DEVICE_NOT_ALLOWED;
using ::chromeos::FORMAT_ERROR_NONE;
using ::chromeos::FORMAT_ERROR_UNKNOWN;
using ::chromeos::FORMAT_ERROR_UNSUPPORTED_FILESYSTEM;
using ::chromeos::FormatError;
using ::chromeos::MOUNT_ACCESS_MODE_READ_ONLY;
using ::chromeos::MOUNT_ACCESS_MODE_READ_WRITE;
using ::chromeos::MOUNT_ERROR_INTERNAL;
using ::chromeos::MOUNT_ERROR_INVALID_DEVICE_PATH;
using ::chromeos::MOUNT_ERROR_INVALID_PATH;
using ::chromeos::MOUNT_ERROR_NONE;
using ::chromeos::MOUNT_ERROR_PATH_ALREADY_MOUNTED;
using ::chromeos::MOUNT_ERROR_PATH_NOT_MOUNTED;
using ::chromeos::MOUNT_ERROR_UNKNOWN;
using ::chromeos::MOUNT_ERROR_UNKNOWN_FILESYSTEM;
using ::chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
using ::chromeos::MOUNT_TYPE_ARCHIVE;
using ::chromeos::MOUNT_TYPE_DEVICE;
using ::chromeos::MountAccessMode;
using ::chromeos::MountEntry;
using ::chromeos::MountError;
using ::chromeos::MountEventType;
using ::chromeos::MountType;
using ::chromeos::PARTITION_ERROR_INVALID_DEVICE_PATH;
using ::chromeos::PARTITION_ERROR_NONE;
using ::chromeos::PARTITION_ERROR_UNKNOWN;
using ::chromeos::PartitionError;
using ::chromeos::REMOUNT_OPTION_MOUNT_NEW_DEVICE;
using ::chromeos::REMOUNT_OPTION_REMOUNT_EXISTING_DEVICE;
using ::chromeos::RENAME_ERROR_DEVICE_NOT_ALLOWED;
using ::chromeos::RENAME_ERROR_NONE;
using ::chromeos::RENAME_ERROR_UNKNOWN;
using ::chromeos::RenameError;
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CROS_DISKS_CROS_DISKS_CLIENT_H_
