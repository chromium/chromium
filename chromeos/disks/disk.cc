// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/disk.h"

#include <utility>

#include "base/memory/ptr_util.h"

namespace chromeos {
namespace disks {

namespace {
constexpr char kStatefulPartition[] = "/mnt/stateful_partition";
}

Disk::Disk(const DiskInfo& disk_info,
           bool write_disabled_by_policy,
           const std::string& base_mount_path)
    : device_path_(disk_info.device_path()),
      mount_path_(disk_info.mount_path()),
      write_disabled_by_policy_(write_disabled_by_policy),
      file_path_(disk_info.file_path()),
      device_label_(disk_info.label()),
      drive_label_(disk_info.drive_label()),
      vendor_id_(disk_info.vendor_id()),
      vendor_name_(disk_info.vendor_name()),
      product_id_(disk_info.product_id()),
      product_name_(disk_info.product_name()),
      fs_uuid_(disk_info.uuid()),
      storage_device_path_(disk_info.storage_device_path()),
      device_type_(disk_info.device_type()),
      total_size_in_bytes_(disk_info.total_size_in_bytes()),
      is_parent_(disk_info.is_drive()),
      is_read_only_hardware_(disk_info.is_read_only()),
      has_media_(disk_info.has_media()),
      on_boot_device_(disk_info.on_boot_device()),
      on_removable_device_(disk_info.on_removable_device()),
      is_hidden_(disk_info.is_hidden()),
      is_auto_mountable_(disk_info.is_auto_mountable()),
      // cros-disks only provides mount paths if the disk is actually mounted.
      is_mounted_(!disk_info.mount_path().empty()),
      file_system_type_(disk_info.file_system_type()),
      base_mount_path_(base_mount_path) {}

Disk::Disk() = default;

Disk::Disk(const Disk&) = default;

Disk::~Disk() = default;

void Disk::SetMountPath(const std::string& mount_path) {
  mount_path_ = mount_path;

  if (base_mount_path_.empty())
    base_mount_path_ = mount_path;
}

bool Disk::IsStatefulPartition() const {
  return mount_path_ == kStatefulPartition;
}

Disk::Builder::Builder() : disk_(base::WrapUnique(new Disk())) {}

Disk::Builder::~Builder() = default;

Disk::Builder& Disk::Builder::SetDevicePath(const std::string& device_path) {
  disk_->device_path_ = device_path;
  return *this;
}

Disk::Builder& Disk::Builder::SetMountPath(const std::string& mount_path) {
  disk_->mount_path_ = mount_path;
  return *this;
}

Disk::Builder& Disk::Builder::SetWriteDisabledByPolicy(
    bool write_disabled_by_policy) {
  disk_->write_disabled_by_policy_ = write_disabled_by_policy;
  return *this;
}
Disk::Builder& Disk::Builder::SetFilePath(const std::string& file_path) {
  disk_->file_path_ = file_path;
  return *this;
}
Disk::Builder& Disk::Builder::SetDeviceLabel(const std::string& device_label) {
  disk_->device_label_ = device_label;
  return *this;
}
Disk::Builder& Disk::Builder::SetDriveLabel(const std::string& drive_label) {
  disk_->drive_label_ = drive_label;
  return *this;
}

Disk::Builder& Disk::Builder::SetVendorId(const std::string& vendor_id) {
  disk_->vendor_id_ = vendor_id;
  return *this;
}

Disk::Builder& Disk::Builder::SetVendorName(const std::string& vendor_name) {
  disk_->vendor_name_ = vendor_name;
  return *this;
}

Disk::Builder& Disk::Builder::SetProductId(const std::string& product_id) {
  disk_->product_id_ = product_id;
  return *this;
}

Disk::Builder& Disk::Builder::SetProductName(const std::string& product_name) {
  disk_->product_name_ = product_name;
  return *this;
}

Disk::Builder& Disk::Builder::SetFileSystemUUID(const std::string& fs_uuid) {
  disk_->fs_uuid_ = fs_uuid;
  return *this;
}

Disk::Builder& Disk::Builder::SetStorageDevicePath(
    const std::string& storage_device_path) {
  disk_->storage_device_path_ = storage_device_path;
  return *this;
}

Disk::Builder& Disk::Builder::SetDeviceType(DeviceType device_type) {
  disk_->device_type_ = device_type;
  return *this;
}

Disk::Builder& Disk::Builder::SetSizeInBytes(uint64_t total_size_in_bytes) {
  disk_->total_size_in_bytes_ = total_size_in_bytes;
  return *this;
}

Disk::Builder& Disk::Builder::SetIsParent(bool is_parent) {
  disk_->is_parent_ = is_parent;
  return *this;
}

Disk::Builder& Disk::Builder::SetIsReadOnlyHardware(
    bool is_read_only_hardware) {
  disk_->is_read_only_hardware_ = is_read_only_hardware;
  return *this;
}

Disk::Builder& Disk::Builder::SetHasMedia(bool has_media) {
  disk_->has_media_ = has_media;
  return *this;
}

Disk::Builder& Disk::Builder::SetOnBootDevice(bool on_boot_device) {
  disk_->on_boot_device_ = on_boot_device;
  return *this;
}

Disk::Builder& Disk::Builder::SetOnRemovableDevice(bool on_removable_device) {
  disk_->on_removable_device_ = on_removable_device;
  return *this;
}

Disk::Builder& Disk::Builder::SetIsHidden(bool is_hidden) {
  disk_->is_hidden_ = is_hidden;
  return *this;
}

Disk::Builder& Disk::Builder::SetFileSystemType(
    const std::string& file_system_type) {
  disk_->file_system_type_ = file_system_type;
  return *this;
}

Disk::Builder& Disk::Builder::SetBaseMountPath(
    const std::string& base_mount_path) {
  disk_->base_mount_path_ = base_mount_path;
  return *this;
}

std::unique_ptr<Disk> Disk::Builder::Build() {
  return std::move(disk_);
}

Disk::Builder& Disk::Builder::SetIsMounted(bool is_mounted) {
  disk_->is_mounted_ = is_mounted;
  return *this;
}

}  // namespace disks
}  // namespace chromeos
