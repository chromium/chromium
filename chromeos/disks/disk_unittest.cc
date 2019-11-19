// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/disk.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "chromeos/dbus/cros_disks_client.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace disks {
namespace {

const char kDevicePath[] = "/sys/device/path";
const char kDeviceFile[] = "/dev/sdb1";
const char kMountPath1[] = "/media/removable/UNTITLED";
const char kMountPath2[] = "/media/removable/second_mount_path";
const char kDriveModel[] = "DriveModel";
const char kIdLabel[] = "UNTITLED";
const char kIdUuid[] = "XXXX-YYYY";
const char kStorageDevicePath[] =
    "/sys/devices/pci0000:00/0000:00:14.0/usb2/2-8/2-8:1.0/host14/target14:0:0/"
    "14:0:0:0";
const char kProductId[] = "1234";
const char kProductName[] = "Product Name";
const char kVendorId[] = "0000";
const char kVendorName[] = "Vendor Name";
const char kFileSystemType[] = "exfat";
const uint64_t kDeviceSize = 16005464064;
const uint32_t kDeviceMediaType = cros_disks::DEVICE_MEDIA_SD;

// Appends a boolean entry to a dictionary of type "a{sv}"
void AppendBoolDictEntry(dbus::MessageWriter* array_writer,
                         const std::string& key,
                         bool value) {
  dbus::MessageWriter entry_writer(nullptr);
  array_writer->OpenDictEntry(&entry_writer);
  entry_writer.AppendString(key);
  entry_writer.AppendVariantOfBool(value);
  array_writer->CloseContainer(&entry_writer);
}

// Appends a string entry to a dictionary of type "a{sv}"
void AppendStringDictEntry(dbus::MessageWriter* array_writer,
                           const std::string& key,
                           const std::string& value) {
  dbus::MessageWriter entry_writer(nullptr);
  array_writer->OpenDictEntry(&entry_writer);
  entry_writer.AppendString(key);
  entry_writer.AppendVariantOfString(value);
  array_writer->CloseContainer(&entry_writer);
}

// Appends a uint64 entry to a dictionary of type "a{sv}"
void AppendUint64DictEntry(dbus::MessageWriter* array_writer,
                           const std::string& key,
                           uint64_t value) {
  dbus::MessageWriter entry_writer(nullptr);
  array_writer->OpenDictEntry(&entry_writer);
  entry_writer.AppendString(key);
  entry_writer.AppendVariantOfUint64(value);
  array_writer->CloseContainer(&entry_writer);
}

// Appends a uint32 entry to a dictionary of type "a{sv}"
void AppendUint32DictEntry(dbus::MessageWriter* array_writer,
                           const std::string& key,
                           uint64_t value) {
  dbus::MessageWriter entry_writer(nullptr);
  array_writer->OpenDictEntry(&entry_writer);
  entry_writer.AppendString(key);
  entry_writer.AppendVariantOfUint32(value);
  array_writer->CloseContainer(&entry_writer);
}

void AppendBasicProperties(dbus::MessageWriter* array_writer) {
  AppendStringDictEntry(array_writer, cros_disks::kDeviceFile, kDeviceFile);
  AppendStringDictEntry(array_writer, cros_disks::kDriveModel, kDriveModel);
  AppendStringDictEntry(array_writer, cros_disks::kIdLabel, kIdLabel);
  AppendStringDictEntry(array_writer, cros_disks::kIdUuid, kIdUuid);
  AppendStringDictEntry(array_writer, cros_disks::kStorageDevicePath,
                        kStorageDevicePath);
  AppendStringDictEntry(array_writer, cros_disks::kProductId, kProductId);
  AppendStringDictEntry(array_writer, cros_disks::kProductName, kProductName);
  AppendStringDictEntry(array_writer, cros_disks::kVendorId, kVendorId);
  AppendStringDictEntry(array_writer, cros_disks::kVendorName, kVendorName);
  AppendStringDictEntry(array_writer, cros_disks::kFileSystemType,
                        kFileSystemType);
  AppendUint64DictEntry(array_writer, cros_disks::kDeviceSize, kDeviceSize);
  AppendUint32DictEntry(array_writer, cros_disks::kDeviceMediaType,
                        kDeviceMediaType);
}

// Builds a dbus reponse with a common set of fields.
std::unique_ptr<dbus::Response> BuildBasicDbusResponse() {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);

  writer.OpenArray("{sv}", &array_writer);
  AppendBasicProperties(&array_writer);
  writer.CloseContainer(&array_writer);

  return response;
}

TEST(DiskTest, ConstructFromDiskInfo) {
  const char kBaseMountpath[] = "/base/mount/path";

  std::unique_ptr<dbus::Response> response = BuildBasicDbusResponse();
  DiskInfo disk_info(kDevicePath, response.get());
  Disk disk(disk_info, false /* write_disabled_by_policy */, kBaseMountpath);

  EXPECT_EQ(kDevicePath, disk.device_path());
  EXPECT_EQ(kDeviceFile, disk.file_path());
  EXPECT_EQ(kIdLabel, disk.device_label());
  EXPECT_EQ(kDriveModel, disk.drive_label());
  EXPECT_EQ(kVendorId, disk.vendor_id());
  EXPECT_EQ(kVendorName, disk.vendor_name());
  EXPECT_EQ(kProductId, disk.product_id());
  EXPECT_EQ(kProductName, disk.product_name());
  EXPECT_EQ(kIdUuid, disk.fs_uuid());
  EXPECT_EQ(kDeviceSize, disk.total_size_in_bytes());
  EXPECT_EQ(DEVICE_TYPE_SD, disk.device_type());
  EXPECT_EQ(kStorageDevicePath, disk.storage_device_path());
  EXPECT_EQ(kBaseMountpath, disk.base_mount_path());
  EXPECT_FALSE(disk.is_parent());
  EXPECT_FALSE(disk.is_read_only());
  EXPECT_FALSE(disk.is_read_only_hardware());
  EXPECT_FALSE(disk.has_media());
  EXPECT_FALSE(disk.on_boot_device());
  EXPECT_FALSE(disk.on_removable_device());
  EXPECT_FALSE(disk.is_mounted());
  EXPECT_FALSE(disk.IsStatefulPartition());
  EXPECT_FALSE(disk.is_auto_mountable());
  EXPECT_TRUE(disk.is_first_mount());

  // Drives are hidden by default.
  EXPECT_TRUE(disk.is_hidden());
}

std::unique_ptr<Disk> BuildDiskWithProperty(const std::string& property,
                                            bool value) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  {
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(nullptr);

    writer.OpenArray("{sv}", &array_writer);
    AppendBasicProperties(&array_writer);
    AppendBoolDictEntry(&array_writer, property, value);
    writer.CloseContainer(&array_writer);
  }
  DiskInfo disk_info(kDevicePath, response.get());
  return std::make_unique<Disk>(disk_info, false, "");
}

TEST(DiskTest, ConstructFromDiskInfo_BoolProperties) {
  {
    auto disk = BuildDiskWithProperty(cros_disks::kDeviceIsDrive, true);
    EXPECT_TRUE(disk->is_parent());
  }
  {
    auto disk = BuildDiskWithProperty(cros_disks::kDeviceIsReadOnly, true);
    EXPECT_TRUE(disk->is_read_only());
    EXPECT_TRUE(disk->is_read_only_hardware());
  }
  {
    auto disk =
        BuildDiskWithProperty(cros_disks::kDeviceIsMediaAvailable, true);
    EXPECT_TRUE(disk->has_media());
  }
  {
    auto disk = BuildDiskWithProperty(cros_disks::kDeviceIsOnBootDevice, true);
    EXPECT_TRUE(disk->on_boot_device());
  }
  {
    auto disk =
        BuildDiskWithProperty(cros_disks::kDeviceIsOnRemovableDevice, true);
    EXPECT_TRUE(disk->on_removable_device());
  }
  {
    auto disk = BuildDiskWithProperty(cros_disks::kIsAutoMountable, true);
    EXPECT_TRUE(disk->is_auto_mountable());
  }
}

TEST(DiskTest, ConstructFromDiskInfo_WriteDisabledByPolicy) {
  std::unique_ptr<dbus::Response> response = BuildBasicDbusResponse();
  DiskInfo disk_info(kDevicePath, response.get());
  Disk disk(disk_info, true /* write_disabled_by_policy */, "");

  EXPECT_TRUE(disk.is_read_only());
  EXPECT_FALSE(disk.is_read_only_hardware());
}

TEST(DiskTest, ConstructFromDiskInfo_Mounted) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  {
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(nullptr);

    writer.OpenArray("{sv}", &array_writer);
    AppendBasicProperties(&array_writer);
    {
      std::vector<std::string> mounted_paths = {kMountPath1, kMountPath2};

      dbus::MessageWriter entry_writer(nullptr);
      array_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(cros_disks::kDeviceMountPaths);
      dbus::MessageWriter variant_writer(nullptr);
      entry_writer.OpenVariant("as", &variant_writer);
      variant_writer.AppendArrayOfStrings(mounted_paths);
      entry_writer.CloseContainer(&variant_writer);
      array_writer.CloseContainer(&entry_writer);
    }
    writer.CloseContainer(&array_writer);
  }

  DiskInfo disk_info(kDevicePath, response.get());
  Disk disk(disk_info, false, "");

  EXPECT_TRUE(disk.is_mounted());
  EXPECT_EQ(kMountPath1, disk.mount_path());
}

TEST(DiskTest, SetMountPath) {
  std::unique_ptr<dbus::Response> response = BuildBasicDbusResponse();
  DiskInfo disk_info(kDevicePath, response.get());
  Disk disk(disk_info, false /* write_disabled_by_policy */, "");

  EXPECT_EQ("", disk.mount_path());
  EXPECT_EQ("", disk.base_mount_path());
  EXPECT_FALSE(disk.is_mounted());

  disk.SetMountPath(kMountPath1);
  EXPECT_EQ(kMountPath1, disk.mount_path());
  EXPECT_EQ(kMountPath1, disk.base_mount_path());
  EXPECT_FALSE(disk.is_mounted());

  disk.set_mounted(true);
  EXPECT_TRUE(disk.is_mounted());
}

}  // namespace
}  // namespace disks
}  // namespace chromeos
