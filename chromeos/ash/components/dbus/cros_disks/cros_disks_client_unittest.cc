// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"

#include <stdint.h>

#include <memory>

#include "base/strings/strcat.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Appends a boolean entry to a dictionary of type "a{sv}"
void AppendBoolDictEntry(dbus::MessageWriter* array_writer,
                         const std::string& key,
                         bool value) {
  dbus::MessageWriter entry_writer(NULL);
  array_writer->OpenDictEntry(&entry_writer);
  entry_writer.AppendString(key);
  entry_writer.AppendVariantOfBool(value);
  array_writer->CloseContainer(&entry_writer);
}

// Appends a string entry to a dictionary of type "a{sv}"
void AppendStringDictEntry(dbus::MessageWriter* array_writer,
                           const std::string& key,
                           const std::string& value) {
  dbus::MessageWriter entry_writer(NULL);
  array_writer->OpenDictEntry(&entry_writer);
  entry_writer.AppendString(key);
  entry_writer.AppendVariantOfString(value);
  array_writer->CloseContainer(&entry_writer);
}

}  // namespace

TEST(CrosDisksClientTest, DiskInfo) {
  const std::string kDeviceFile = "/dev/sdb1";
  const bool kDeviceIsDrive = true;
  const bool kDeviceIsMediaAvailable = true;
  const bool kDeviceIsMounted = true;
  const bool kDeviceIsOnBootDevice = true;
  const bool kDeviceIsOnRemovableDevice = true;
  const bool kDeviceIsReadOnly = true;
  const uint32_t kDeviceMediaType = static_cast<uint32_t>(ash::DeviceType::kSD);
  const std::string kMountPath = "/media/removable/UNTITLED";
  const bool kDevicePresentationHide = false;
  const uint64_t kDeviceSize = 16005464064;
  const std::string kDriveModel = "DriveModel";
  const std::string kIdLabel = "UNTITLED";
  const std::string kIdUuid = "XXXX-YYYY";
  const std::string kProductId = "1234";
  const std::string kProductName = "Product Name";
  const std::string kVendorId = "0000";
  const std::string kVendorName = "Vendor Name";
  const std::string kFileSystemType = "exfat";
  const bool kIsAutoMountable = true;

  // Construct a fake response of GetDeviceProperties().
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  {
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(NULL);
    writer.OpenArray("{sv}", &array_writer);

    AppendStringDictEntry(&array_writer, cros_disks::kDeviceFile, kDeviceFile);
    AppendBoolDictEntry(&array_writer, cros_disks::kDeviceIsDrive,
                        kDeviceIsDrive);
    AppendBoolDictEntry(&array_writer, cros_disks::kDeviceIsMediaAvailable,
                        kDeviceIsMediaAvailable);
    AppendBoolDictEntry(&array_writer, cros_disks::kDeviceIsMounted,
                        kDeviceIsMounted);
    AppendBoolDictEntry(&array_writer, cros_disks::kDeviceIsOnBootDevice,
                        kDeviceIsOnBootDevice);
    AppendBoolDictEntry(&array_writer, cros_disks::kDeviceIsOnRemovableDevice,
                        kDeviceIsOnRemovableDevice);
    AppendBoolDictEntry(&array_writer, cros_disks::kDeviceIsReadOnly,
                        kDeviceIsReadOnly);
    AppendBoolDictEntry(&array_writer, cros_disks::kIsAutoMountable,
                        kIsAutoMountable);
    {
      dbus::MessageWriter entry_writer(NULL);
      array_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(cros_disks::kDeviceMediaType);
      entry_writer.AppendVariantOfUint32(kDeviceMediaType);
      array_writer.CloseContainer(&entry_writer);
    }
    {
      std::vector<std::string> mounted_paths;
      mounted_paths.push_back(kMountPath);

      dbus::MessageWriter entry_writer(NULL);
      array_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(cros_disks::kDeviceMountPaths);
      dbus::MessageWriter variant_writer(NULL);
      entry_writer.OpenVariant("as", &variant_writer);
      variant_writer.AppendArrayOfStrings(mounted_paths);
      entry_writer.CloseContainer(&variant_writer);
      array_writer.CloseContainer(&entry_writer);
    }
    AppendBoolDictEntry(&array_writer, cros_disks::kDevicePresentationHide,
                        kDevicePresentationHide);
    {
      dbus::MessageWriter entry_writer(NULL);
      array_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(cros_disks::kDeviceSize);
      entry_writer.AppendVariantOfUint64(kDeviceSize);
      array_writer.CloseContainer(&entry_writer);
    }
    AppendStringDictEntry(&array_writer, cros_disks::kDriveModel, kDriveModel);
    AppendStringDictEntry(&array_writer, cros_disks::kIdLabel, kIdLabel);
    AppendStringDictEntry(&array_writer, cros_disks::kIdUuid, kIdUuid);
    AppendStringDictEntry(&array_writer, cros_disks::kProductId, kProductId);
    AppendStringDictEntry(&array_writer, cros_disks::kProductName,
                          kProductName);
    AppendStringDictEntry(&array_writer, cros_disks::kVendorId, kVendorId);
    AppendStringDictEntry(&array_writer, cros_disks::kVendorName, kVendorName);
    AppendStringDictEntry(&array_writer, cros_disks::kFileSystemType,
                          kFileSystemType);

    writer.CloseContainer(&array_writer);
  }

  // Construct DiskInfo.
  DiskInfo result(kDeviceFile, response.get());
  EXPECT_EQ(kDeviceFile, result.device_path());
  EXPECT_EQ(kDeviceIsDrive, result.is_drive());
  EXPECT_EQ(kDeviceIsReadOnly, result.is_read_only());
  // Use EXPECT_TRUE(kDevicePresentationHide == result.is_hidden()) instead of
  // EXPECT_EQ(kDevicePresentationHide, result.is_hidden()) as gcc 4.7 issues
  // the following warning on EXPECT_EQ(false, x), which is turned into an error
  // with -Werror=conversion-null:
  //
  //   converting 'false' to pointer type for argument 1 of
  //   'char testing::internal::IsNullLiteralHelper(testing::internal::Secret*)'
  EXPECT_TRUE(kDevicePresentationHide == result.is_hidden());
  EXPECT_EQ(kDeviceIsMediaAvailable, result.has_media());
  EXPECT_EQ(kDeviceIsOnBootDevice, result.on_boot_device());
  EXPECT_EQ(kDeviceIsOnRemovableDevice, result.on_removable_device());
  EXPECT_EQ(kDeviceFile, result.file_path());
  EXPECT_EQ(kVendorId, result.vendor_id());
  EXPECT_EQ(kVendorName, result.vendor_name());
  EXPECT_EQ(kProductId, result.product_id());
  EXPECT_EQ(kProductName, result.product_name());
  EXPECT_EQ(kDriveModel, result.drive_label());
  EXPECT_EQ(kIdLabel, result.label());
  EXPECT_EQ(kIdUuid, result.uuid());
  EXPECT_EQ(kDeviceSize, result.total_size_in_bytes());
  EXPECT_EQ(DeviceType::kSD, result.device_type());
  EXPECT_EQ(kMountPath, result.mount_path());
  EXPECT_EQ(kFileSystemType, result.file_system_type());
  EXPECT_EQ(kIsAutoMountable, result.is_auto_mountable());
}

TEST(CrosDisksClientTest, ComposeMountOptions) {
  using testing::UnorderedElementsAre;
  const std::string label = "/tmp/cros-disks-test";
  const std::string label_option = base::StrCat({"mountlabel=", label});

  // TODO(b/364409158) Remove with files-kernel-drivers feature flag.
  const std::string driver_option = "prefer-driver=kernel";

  EXPECT_THAT(CrosDisksClient::ComposeMountOptions(
                  {}, label, MountAccessMode::kReadWrite,
                  RemountOption::kMountNewDevice),
              UnorderedElementsAre("rw", label_option, driver_option));

  EXPECT_THAT(CrosDisksClient::ComposeMountOptions(
                  {}, label, MountAccessMode::kReadOnly,
                  RemountOption::kMountNewDevice),
              UnorderedElementsAre("ro", label_option, driver_option));

  EXPECT_THAT(
      CrosDisksClient::ComposeMountOptions(
          {}, label, MountAccessMode::kReadWrite,
          RemountOption::kRemountExistingDevice),
      UnorderedElementsAre("rw", "remount", label_option, driver_option));

  EXPECT_THAT(CrosDisksClient::ComposeMountOptions(
                  {"foo", "bar=baz"}, label, MountAccessMode::kReadWrite,
                  RemountOption::kMountNewDevice),
              UnorderedElementsAre("foo", "bar=baz", "rw", label_option,
                                   driver_option));
}

}  // namespace ash
