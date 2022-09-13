// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestPortableDeviceWatcherWin implementation.

#include "components/storage_monitor/test_portable_device_watcher_win.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"

namespace storage_monitor {

namespace {

// Sample MTP device storage information.
const wchar_t kMTPDeviceFriendlyName[] = L"Camera V1.1";
const wchar_t kStorageLabelA[] = L"Camera V1.1 (s10001)";
const wchar_t kStorageLabelB[] = L"Camera V1.1 (s20001)";
const wchar_t kStorageObjectIdA[] = L"s10001";
const wchar_t kStorageObjectIdB[] = L"s20001";
const char kStorageUniqueIdB[] =
    "mtp:StorageSerial:SID-{s20001, S, 2238}:123123";

// Returns the storage name of the device specified by |pnp_device_id|.
// |storage_object_id| specifies the string ID that uniquely identifies the
// object on the device.
std::wstring GetMTPStorageName(const std::wstring& pnp_device_id,
                               const std::wstring& storage_object_id) {
  if (pnp_device_id == TestPortableDeviceWatcherWin::kMTPDeviceWithInvalidInfo)
    return std::wstring();

  if (storage_object_id == kStorageObjectIdA)
    return kStorageLabelA;
  return (storage_object_id == kStorageObjectIdB) ? kStorageLabelB
                                                  : std::wstring();
}

}  // namespace

// TestPortableDeviceWatcherWin ------------------------------------------------

// static
const wchar_t TestPortableDeviceWatcherWin::kMTPDeviceWithMultipleStorages[] =
    L"\\?\\usb#vid_ff&pid_18#32&2&1#{ab33-1de4-f22e-1882-9724})";
const wchar_t TestPortableDeviceWatcherWin::kMTPDeviceWithInvalidInfo[] =
    L"\\?\\usb#vid_00&pid_00#0&2&1#{0000-0000-0000-0000-0000})";
const wchar_t TestPortableDeviceWatcherWin::kMTPDeviceWithValidInfo[] =
    L"\\?\\usb#vid_ff&pid_000f#32&2&1#{abcd-1234-ffde-1112-9172})";
const char TestPortableDeviceWatcherWin::kStorageUniqueIdA[] =
    "mtp:StorageSerial:SID-{s10001, D, 12378}:123123";

TestPortableDeviceWatcherWin::TestPortableDeviceWatcherWin()
    : use_dummy_mtp_storage_info_(false) {
}

TestPortableDeviceWatcherWin::~TestPortableDeviceWatcherWin() {
}

// static
std::string TestPortableDeviceWatcherWin::GetMTPStorageUniqueId(
    const std::wstring& pnp_device_id,
    const std::wstring& storage_object_id) {
  if (storage_object_id == kStorageObjectIdA)
    return TestPortableDeviceWatcherWin::kStorageUniqueIdA;
  return (storage_object_id == kStorageObjectIdB) ?
      kStorageUniqueIdB : std::string();
}

// static
PortableDeviceWatcherWin::StorageObjectIDs
TestPortableDeviceWatcherWin::GetMTPStorageObjectIds(
    const std::wstring& pnp_device_id) {
  PortableDeviceWatcherWin::StorageObjectIDs storage_object_ids;
  storage_object_ids.push_back(kStorageObjectIdA);
  if (pnp_device_id == kMTPDeviceWithMultipleStorages)
    storage_object_ids.push_back(kStorageObjectIdB);
  return storage_object_ids;
}

// static
void TestPortableDeviceWatcherWin::GetMTPStorageDetails(
    const std::wstring& pnp_device_id,
    const std::wstring& storage_object_id,
    std::wstring* device_location,
    std::string* unique_id,
    std::wstring* name) {
  std::string storage_unique_id = GetMTPStorageUniqueId(pnp_device_id,
                                                        storage_object_id);
  if (device_location)
    *device_location = base::UTF8ToWide("\\\\" + storage_unique_id);

  if (unique_id)
    *unique_id = storage_unique_id;

  if (name)
    *name = GetMTPStorageName(pnp_device_id, storage_object_id);
}

// static
PortableDeviceWatcherWin::StorageObjects
TestPortableDeviceWatcherWin::GetDeviceStorageObjects(
    const std::wstring& pnp_device_id) {
  PortableDeviceWatcherWin::StorageObjects storage_objects;
  PortableDeviceWatcherWin::StorageObjectIDs storage_object_ids =
      GetMTPStorageObjectIds(pnp_device_id);
  for (PortableDeviceWatcherWin::StorageObjectIDs::const_iterator it =
           storage_object_ids.begin();
       it != storage_object_ids.end(); ++it) {
    storage_objects.push_back(DeviceStorageObject(
        *it, GetMTPStorageUniqueId(pnp_device_id, *it)));
  }
  return storage_objects;
}

void TestPortableDeviceWatcherWin::EnumerateAttachedDevices() {
}

void TestPortableDeviceWatcherWin::HandleDeviceAttachEvent(
    const std::wstring& pnp_device_id) {
  DeviceDetails device_details;
  if (pnp_device_id != kMTPDeviceWithInvalidInfo)
    device_details.name = kMTPDeviceFriendlyName;
  device_details.location = pnp_device_id;
  device_details.storage_objects = GetDeviceStorageObjects(pnp_device_id);
  OnDidHandleDeviceAttachEvent(&device_details, true);
}

bool TestPortableDeviceWatcherWin::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    std::wstring* device_location,
    std::wstring* storage_object_id) const {
  DCHECK(!storage_device_id.empty());
  if (use_dummy_mtp_storage_info_) {
    if (storage_device_id == TestPortableDeviceWatcherWin::kStorageUniqueIdA) {
      *device_location = kMTPDeviceWithValidInfo;
      *storage_object_id = kStorageObjectIdA;
      return true;
    }
    return false;
  }
  return PortableDeviceWatcherWin::GetMTPStorageInfoFromDeviceId(
      storage_device_id, device_location, storage_object_id);
}

}  // namespace storage_monitor
