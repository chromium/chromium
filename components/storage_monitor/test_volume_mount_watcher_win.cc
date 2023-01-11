// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestVolumeMountWatcherWin implementation.

#include "components/storage_monitor/test_volume_mount_watcher_win.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/storage_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_monitor {

namespace {

base::FilePath GetTempRoot() {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_root = temp_dir.GetPath();
  while (temp_root.DirName() != temp_root)
    temp_root = temp_root.DirName();
  return temp_root;
}

std::vector<base::FilePath> FakeGetSingleAttachedDevice() {
  std::vector<base::FilePath> result;
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(2));  // C

  // Make sure we are adding the drive on which ScopedTempDir will make
  // test directories.
  base::FilePath temp_root = GetTempRoot();
  if (temp_root != VolumeMountWatcherWin::DriveNumberToFilePath(2))
    result.push_back(temp_root);

  return result;
}

std::vector<base::FilePath> FakeGetAttachedDevices() {
  std::vector<base::FilePath> result;
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(0));  // A
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(1));  // B
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(2));  // C
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(3));  // D
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(5));  // F
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(7));  // H
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(13));  // N
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(25));  // Z
  return result;
}

// Gets the details of the mass storage device specified by the |device_path|.
// |device_path| inputs of 'A:\' - 'Z:\' are valid. 'N:\' is not removable.
// 'C:\' is not removable (so that auto-added paths are correctly handled).
bool GetMassStorageDeviceDetails(const base::FilePath& device_path,
                                 StorageInfo* info) {
  DCHECK(info);

  // Truncate to root path.
  base::FilePath path(device_path);
  if (device_path.value().length() > 3)
    path = base::FilePath(device_path.value().substr(0, 3));
  base::FilePath::CharType drive_letter = path.value()[0];
  if (drive_letter < L'A' || drive_letter > L'Z')
    return false;

  StorageInfo::Type type = StorageInfo::FIXED_MASS_STORAGE;
  if (path.value() != L"N:\\" && path.value() != L"C:\\" &&
      path.value() != GetTempRoot().value()) {
    type = StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  }
  std::string unique_id =
      "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\";
  unique_id[11] = static_cast<char>(drive_letter);
  std::string device_id = StorageInfo::MakeDeviceId(type, unique_id);
  std::u16string storage_label = path.Append(L" Drive").LossyDisplayName();
  *info = StorageInfo(device_id, path.value(), storage_label, std::u16string(),
                      std::u16string(), 1000000);

  return true;
}

}  // namespace

// TestVolumeMountWatcherWin ---------------------------------------------------

TestVolumeMountWatcherWin::TestVolumeMountWatcherWin()
    : attached_devices_fake_(false) {}

TestVolumeMountWatcherWin::~TestVolumeMountWatcherWin() {
}

void TestVolumeMountWatcherWin::AddDeviceForTesting(
    const base::FilePath& device_path,
    const std::string& device_id,
    const std::u16string& storage_label,
    uint64_t total_size_in_bytes) {
  StorageInfo info(device_id, device_path.value(), storage_label,
                   std::u16string(), std::u16string(), total_size_in_bytes);
  HandleDeviceAttachEventOnUIThread(device_path, info);
}

void TestVolumeMountWatcherWin::SetAttachedDevicesFake() {
  attached_devices_fake_ = true;
}

void TestVolumeMountWatcherWin::DeviceCheckComplete(
    const base::FilePath& device_path) {
  devices_checked_.push_back(device_path);
  if (device_check_complete_event_)
    device_check_complete_event_->Wait();
  VolumeMountWatcherWin::DeviceCheckComplete(device_path);
}

void TestVolumeMountWatcherWin::BlockDeviceCheckForTesting() {
  device_check_complete_event_ = std::make_unique<base::WaitableEvent>(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  devices_checked_.clear();
}

void TestVolumeMountWatcherWin::ReleaseDeviceCheck() {
  device_check_complete_event_->Signal();
}

// static
bool TestVolumeMountWatcherWin::GetDeviceRemovable(
    const base::FilePath& device_path,
    bool* removable) {
  StorageInfo info;
  bool success = GetMassStorageDeviceDetails(device_path, &info);
  *removable = StorageInfo::IsRemovableDevice(info.device_id());
  return success;
}

VolumeMountWatcherWin::GetDeviceDetailsCallbackType
TestVolumeMountWatcherWin::GetDeviceDetailsCallback() const {
  return base::BindOnce(&GetMassStorageDeviceDetails);
}

VolumeMountWatcherWin::GetAttachedDevicesCallbackType
  TestVolumeMountWatcherWin::GetAttachedDevicesCallback() const {
  if (attached_devices_fake_)
    return base::BindOnce(&FakeGetAttachedDevices);
  return base::BindOnce(&FakeGetSingleAttachedDevice);
}

}  // namespace storage_monitor
