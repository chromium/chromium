// Copyright 2012 The Chromium Authors
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
#include "base/strings/string_piece.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

namespace base {
class FilePath;
}

namespace dbus {
class Response;
}

// TODO(crbug.com/1368408): Most of these are partially or completely duplicated
// in third_party/dbus/service_constants.h. We should probably use enums from
// service_contstants directly.
namespace ash {

// Enum describing types of mount used by cros-disks.
enum class MountType {
  kInvalid,
  kDevice,
  kArchive,
  kNetworkStorage,
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, MountType type);

// Type of device.
enum class DeviceType {
  kUnknown,
  kUSB,          // USB stick.
  kSD,           // SD card.
  kOpticalDisc,  // e.g. Optical disc excluding DVD.
  kMobile,       // Storage on a mobile device (e.g. Android).
  kDVD,          // DVD.
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, DeviceType type);

// Mount error code used by cros-disks.
// These values are NOT the same as cros_disks::MountErrorType.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MountError {
  kNone = 0,
  kUnknown = 1,
  kInternal = 2,
  kInvalidArgument = 3,
  kInvalidPath = 4,
  kPathAlreadyMounted = 5,
  kPathNotMounted = 6,
  kDirectoryCreationFailed = 7,
  kInvalidMountOptions = 8,
  kInvalidUnmountOptions = 9,
  kInsufficientPermissions = 10,
  kMountProgramNotFound = 11,
  kMountProgramFailed = 12,
  kInvalidDevicePath = 13,
  kUnknownFilesystem = 14,
  kUnsupportedFilesystem = 15,
  kInvalidArchive = 16,
  kNeedPassword = 17,
  kInProgress = 18,
  kCancelled = 19,
  kBusy = 20,
  kMaxValue = 20,
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, MountError error);

// Rename error reported by cros-disks.
enum class RenameError {
  kNone,
  kUnknown,
  kInternal,
  kInvalidDevicePath,
  kDeviceBeingRenamed,
  kUnsupportedFilesystem,
  kRenameProgramNotFound,
  kRenameProgramFailed,
  kDeviceNotAllowed,
  kLongName,
  kInvalidCharacter,
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, RenameError error);

// Format error reported by cros-disks.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See enum CrosDisksClientFormatError in tools/metrics/histograms/enums.xml.
enum class FormatError {
  kNone = 0,
  kUnknown = 1,
  kInternal = 2,
  kInvalidDevicePath = 3,
  kDeviceBeingFormatted = 4,
  kUnsupportedFilesystem = 5,
  kFormatProgramNotFound = 6,
  kFormatProgramFailed = 7,
  kDeviceNotAllowed = 8,
  kInvalidOptions = 9,
  kLongName = 10,
  kInvalidCharacter = 11,
  kMaxValue = 11,
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, FormatError error);

// Partition error reported by cros-disks.
enum class PartitionError {
  kNone = 0,
  kUnknown = 1,
  kInternal = 2,
  kInvalidDevicePath = 3,
  kDeviceBeingPartitioned = 4,
  kProgramNotFound = 5,
  kProgramFailed = 6,
  kDeviceNotAllowed = 7,
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, PartitionError error);

// Event type each corresponding to a signal sent from cros-disks.
enum class MountEventType {
  kDiskAdded,
  kDiskRemoved,
  kDiskChanged,
  kDeviceAdded,
  kDeviceRemoved,
  kDeviceScanned,
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, MountEventType event);

// Mount option to control write permission to a device.
enum class MountAccessMode {
  kReadWrite,
  kReadOnly,
};

// Whether to mount to a new path or remount a device already mounted.
enum RemountOption {
  // Mount a new device. If the device is already mounted, the mount status
  // is
  // unchanged and the callback for MountCompleted will receive
  // MountError::kPathAlreadyMounted error code.
  kMountNewDevice,
  // Remount a device that is already mounted. If the device is not mounted
  // yet, it will do nothing and the callback for MountCompleted will
  // receive
  // MountError::kPathNotMounted error code.
  kRemountExistingDevice,
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
  bool InitializeFromResponse(dbus::Response* response);

  std::string device_path_;
  std::string mount_path_;
  std::string storage_device_path_;
  std::string file_path_;
  std::string label_;
  std::string vendor_id_;
  std::string vendor_name_;
  std::string product_id_;
  std::string product_name_;
  std::string drive_model_;
  std::string uuid_;
  std::string file_system_type_;
  uint64_t total_size_in_bytes_ = 0;
  DeviceType device_type_ = DeviceType::kUnknown;
  int bus_number_ = 0;
  int device_number_ = 0;
  bool is_drive_ = false;
  bool has_media_ = false;
  bool on_boot_device_ = false;
  bool on_removable_device_ = false;
  bool is_read_only_ = false;
  bool is_hidden_ = true;
  bool is_virtual_ = false;
  bool is_auto_mountable_ = false;
};

// A struct to represent information about a mount point sent from cros-disks.
struct COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS) MountPoint {
  // Device or archive path.
  std::string source_path;
  // Mounted path.
  std::string mount_path;
  // Type of mount.
  MountType mount_type = MountType::kInvalid;
  // Condition of mount.
  MountError mount_error = MountError::kNone;
  // Progress percent between 0 and 100 when mount_error is kInProgress.
  int progress_percent = 0;
  // Read-only file system?
  bool read_only = false;

  MountPoint(const MountPoint&);
  MountPoint& operator=(const MountPoint&);

  MountPoint(MountPoint&&);
  MountPoint& operator=(MountPoint&&);

  MountPoint();
  MountPoint(base::StringPiece source_path,
             base::StringPiece mount_path,
             MountType mount_type = MountType::kInvalid,
             MountError mount_error = MountError::kNone,
             int progress_percent = 0,
             bool read_only = false);
};

// Output operator for logging.
COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS)
std::ostream& operator<<(std::ostream& out, const MountPoint& entry);

// A class to make the actual DBus calls for cros-disks service.
// This class only makes calls, result/error handling should be done
// by callbacks.
class COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS) CrosDisksClient
    : public chromeos::DBusClient {
 public:
  // A callback to handle the result of EnumerateDevices.
  // The argument is the enumerated device paths.
  typedef base::OnceCallback<void(const std::vector<std::string>& device_paths)>
      EnumerateDevicesCallback;

  // A callback to handle the result of EnumerateMountEntries.
  // The argument is the enumerated mount entries.
  typedef base::OnceCallback<void(const std::vector<MountPoint>& entries)>
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
    virtual void OnMountCompleted(const MountPoint& entry) = 0;

    // Called when a MountProgress signal is received.
    virtual void OnMountProgress(const MountPoint& entry) = 0;

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
                     chromeos::VoidDBusMethodCallback callback) = 0;

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
                      chromeos::VoidDBusMethodCallback callback) = 0;

  // Calls SinglePartitionFormat async method. |callback| is called when
  // response received.
  virtual void SinglePartitionFormat(const std::string& device_path,
                                     PartitionCallback callback) = 0;

  // Calls Rename method. On completion, |callback| is called, with |true| on
  // success, or with |false| otherwise.
  virtual void Rename(const std::string& device_path,
                      const std::string& volume_name,
                      chromeos::VoidDBusMethodCallback callback) = 0;

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

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CROS_DISKS_CROS_DISKS_CLIENT_H_
