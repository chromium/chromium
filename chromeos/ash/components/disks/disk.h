// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DISKS_DISK_H_
#define CHROMEOS_ASH_COMPONENTS_DISKS_DISK_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"

namespace ash {
namespace disks {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DISKS) Disk {
 public:
  class Builder;

  Disk(const DiskInfo& disk_info,
       // Whether the device is mounted in read-only mode by the policy.
       // Valid only when the device mounted and mount_path_ is non-empty.
       bool write_disabled_by_policy,
       const std::string& base_mount_path);

  // For tests.
  // TODO(amistry): Eliminate this copy constructor. It is only used in tests.
  Disk(const Disk&);

  ~Disk();

  // The path of the device, used by devicekit-disks.
  // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/block/sdb/sdb1)
  const std::string& device_path() const { return device_path_; }

  // The path to the mount point of this device. Will be empty if not mounted.
  // (e.g. /media/removable/VOLUME)
  // TODO(amistry): mount_path() being set DOES NOT means the disk is mounted.
  // See crrev.com/f8692888d11a10b5b5f8ad6fbfdeae21aed8cbf6 for the reason.
  const std::string& mount_path() const { return mount_path_; }

  // The path of the device according to filesystem.
  // (e.g. /dev/sdb)
  const std::string& file_path() const { return file_path_; }

  // Device's label.
  const std::string& device_label() const { return device_label_; }

  void set_device_label(const std::string& device_label) {
    device_label_ = device_label;
  }

  // If disk is a parent, then its label, else parents label.
  // (e.g. "TransMemory")
  const std::string& drive_label() const { return drive_label_; }

  // Vendor ID of the device (e.g. "18d1").
  const std::string& vendor_id() const { return vendor_id_; }

  // Vendor name of the device (e.g. "Google Inc.").
  const std::string& vendor_name() const { return vendor_name_; }

  // Product ID of the device (e.g. "4e11").
  const std::string& product_id() const { return product_id_; }

  // Product name of the device (e.g. "Nexus One").
  const std::string& product_name() const { return product_name_; }

  // Returns the file system uuid string.
  const std::string& fs_uuid() const { return fs_uuid_; }

  // Path of the storage device this device's block is a part of.
  // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/)
  const std::string& storage_device_path() const {
    return storage_device_path_;
  }

  // Device type.
  DeviceType device_type() const { return device_type_; }

  // USB bus number of the device.
  int bus_number() const { return bus_number_; }

  // USB device number of the device.
  int device_number() const { return device_number_; }

  // Total size of the device in bytes.
  uint64_t total_size_in_bytes() const { return total_size_in_bytes_; }

  // Is the device is a parent device (i.e. sdb rather than sdb1).
  bool is_parent() const { return is_parent_; }

  // Whether the user can write to the device. True if read-only.
  bool is_read_only() const {
    return is_read_only_hardware_ || write_disabled_by_policy_;
  }

  // Is the device read only.
  bool is_read_only_hardware() const { return is_read_only_hardware_; }

  // Does the device contains media.
  bool has_media() const { return has_media_; }

  // Is the device on the boot device.
  bool on_boot_device() const { return on_boot_device_; }

  // Is the device on the removable device.
  bool on_removable_device() const { return on_removable_device_; }

  // Shoud the device be shown in the UI, or automounted.
  bool is_hidden() const { return is_hidden_; }

  // Is the disk auto-mountable.
  bool is_auto_mountable() const { return is_auto_mountable_; }

  void set_write_disabled_by_policy(bool disable) {
    write_disabled_by_policy_ = disable;
  }

  void clear_mount_path() { mount_path_.clear(); }

  bool is_mounted() const { return is_mounted_; }

  void set_mounted(bool mounted) { is_mounted_ = mounted; }

  const std::string& file_system_type() const { return file_system_type_; }

  void set_file_system_type(const std::string& file_system_type) {
    file_system_type_ = file_system_type;
  }
  // Name of the first mount path of the disk.
  const std::string& base_mount_path() const { return base_mount_path_; }

  void SetMountPath(const std::string& mount_path);

  bool IsStatefulPartition() const;

  // Is the disk being mounted for the first time since being plugged in.
  bool is_first_mount() const { return is_first_mount_; }

  void set_is_first_mount(bool first_mount) { is_first_mount_ = first_mount; }

 private:
  friend class Builder;

  Disk();

  std::string device_path_;
  std::string mount_path_;
  bool write_disabled_by_policy_ = false;
  std::string file_path_;
  std::string device_label_;
  std::string drive_label_;
  std::string vendor_id_;
  std::string vendor_name_;
  std::string product_id_;
  std::string product_name_;
  std::string fs_uuid_;
  std::string storage_device_path_;
  DeviceType device_type_ = DeviceType::kUnknown;
  int bus_number_ = 0;
  int device_number_ = 0;
  uint64_t total_size_in_bytes_ = 0;
  bool is_parent_ = false;
  bool is_read_only_hardware_ = false;
  bool has_media_ = false;
  bool on_boot_device_ = false;
  bool on_removable_device_ = false;
  bool is_hidden_ = false;
  bool is_auto_mountable_ = false;
  bool is_mounted_ = false;
  bool is_first_mount_ = true;
  std::string file_system_type_;
  std::string base_mount_path_;
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DISKS) Disk::Builder {
 public:
  Builder();

  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  ~Builder();

  Builder& SetDevicePath(const std::string& device_path);
  Builder& SetMountPath(const std::string& mount_path);
  Builder& SetWriteDisabledByPolicy(bool write_disabled_by_policy);
  Builder& SetFilePath(const std::string& file_path);
  Builder& SetDeviceLabel(const std::string& device_label);
  Builder& SetDriveLabel(const std::string& drive_label);
  Builder& SetVendorId(const std::string& vendor_id);
  Builder& SetVendorName(const std::string& vendor_name);
  Builder& SetProductId(const std::string& product_id);
  Builder& SetProductName(const std::string& product_name);
  Builder& SetFileSystemUUID(const std::string& fs_uuid);
  Builder& SetStorageDevicePath(const std::string& storage_device_path_);
  Builder& SetDeviceType(DeviceType device_type);
  Builder& SetBusNumber(int bus_number);
  Builder& SetDeviceNumber(int device_number);
  Builder& SetSizeInBytes(uint64_t total_size_in_bytes);
  Builder& SetIsParent(bool is_parent);
  Builder& SetIsReadOnlyHardware(bool is_read_only_hardware);
  Builder& SetHasMedia(bool has_media);
  Builder& SetOnBootDevice(bool on_boot_device);
  Builder& SetOnRemovableDevice(bool on_removable_device);
  Builder& SetIsHidden(bool is_hidden);
  Builder& SetFileSystemType(const std::string& file_system_type);
  Builder& SetBaseMountPath(const std::string& base_mount_path);
  Builder& SetIsMounted(bool is_mounted);

  std::unique_ptr<Disk> Build();

 private:
  std::unique_ptr<Disk> disk_;
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DISKS)
base::FilePath GetStatefulPartitionPath();

}  // namespace disks
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DISKS_DISK_H_
