// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MtpManagerClientChromeOS unit tests.

#include "components/storage_monitor/mtp_manager_client_chromeos.h"

#include <memory>
#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/mock_removable_storage_observer.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_info_utils.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_monitor {

namespace {

// Sample MTP device storage information.
const char kStorageWithInvalidInfo[] = "usb:2,3:11111";
const char kStorageWithValidInfo[] = "usb:2,2:88888";
const char kStorageVendor[] = "ExampleVendor";
const char16_t kStorageVendor16[] = u"ExampleVendor";
const uint32_t kStorageVendorId = 0x040a;
const char kStorageProduct[] = "ExampleCamera";
const char16_t kStorageProduct16[] = u"ExampleCamera";
const uint32_t kStorageProductId = 0x0160;
const uint32_t kStorageDeviceFlags = 0x0004000;
const uint32_t kStorageType = 3;                         // Fixed RAM
const uint32_t kStorageFilesystemType = 2;               // Generic Hierarchical
const uint32_t kStorageAccessCapability = 0;             // Read-Write
const uint64_t kStorageMaxCapacity = 0x40000000;         // 1G in total
const uint64_t kStorageFreeSpaceInBytes = 0x20000000;    // 512M bytes left
const uint64_t kStorageFreeSpaceInObjects = 0x04000000;  // 64M Objects left
const char kStorageDescription[] = "ExampleDescription";
const char kStorageVolumeIdentifier[] = "ExampleVolumeId";
const char kStorageSerialNumber[] = "0123456789ABCDEF0123456789ABCDEF";

base::LazyInstance<std::map<std::string, device::mojom::MtpStorageInfo>>::Leaky
    g_fake_storage_info_map = LAZY_INSTANCE_INITIALIZER;

const device::mojom::MtpStorageInfo* GetFakeMtpStorageInfoSync(
    const std::string& storage_name) {
  // Fill the map out if it is empty.
  if (g_fake_storage_info_map.Get().empty()) {
    // Add the invalid MTP storage info.
    auto storage_info = device::mojom::MtpStorageInfo();
    storage_info.storage_name = kStorageWithInvalidInfo;
    g_fake_storage_info_map.Get().insert(
        std::make_pair(kStorageWithInvalidInfo, storage_info));
    // Add the valid MTP storage info.
    g_fake_storage_info_map.Get().insert(std::make_pair(
        kStorageWithValidInfo,
        device::mojom::MtpStorageInfo(
            kStorageWithValidInfo, kStorageVendor, kStorageVendorId,
            kStorageProduct, kStorageProductId, kStorageDeviceFlags,
            kStorageType, kStorageFilesystemType, kStorageAccessCapability,
            kStorageMaxCapacity, kStorageFreeSpaceInBytes,
            kStorageFreeSpaceInObjects, kStorageDescription,
            kStorageVolumeIdentifier, kStorageSerialNumber)));
  }

  const auto it = g_fake_storage_info_map.Get().find(storage_name);
  return it != g_fake_storage_info_map.Get().end() ? &it->second : nullptr;
}

class FakeMtpManagerClientChromeOS : public MtpManagerClientChromeOS {
 public:
  FakeMtpManagerClientChromeOS(StorageMonitor::Receiver* receiver,
                               device::mojom::MtpManager* mtp_manager)
      : MtpManagerClientChromeOS(receiver, mtp_manager) {}

  FakeMtpManagerClientChromeOS(const FakeMtpManagerClientChromeOS&) = delete;
  FakeMtpManagerClientChromeOS& operator=(const FakeMtpManagerClientChromeOS&) =
      delete;

  // Notifies MtpManagerClientChromeOS about the attachment of MTP storage
  // device given the |storage_name|.
  void MtpStorageAttached(const std::string& storage_name) {
    auto* storage_info = GetFakeMtpStorageInfoSync(storage_name);
    DCHECK(storage_info);

    StorageAttached(storage_info->Clone());
    base::RunLoop().RunUntilIdle();
  }

  // Notifies MtpManagerClientChromeOS about the detachment of MTP storage
  // device given the |storage_name|.
  void MtpStorageDetached(const std::string& storage_name) {
    StorageDetached(storage_name);
    base::RunLoop().RunUntilIdle();
  }
};

}  // namespace

// A class to test the functionality of MtpManagerClientChromeOS member
// functions.
class MtpManagerClientChromeOSTest : public testing::Test {
 public:
  MtpManagerClientChromeOSTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  MtpManagerClientChromeOSTest(const MtpManagerClientChromeOSTest&) = delete;
  MtpManagerClientChromeOSTest& operator=(const MtpManagerClientChromeOSTest&) =
      delete;

  ~MtpManagerClientChromeOSTest() override = default;

 protected:
  void SetUp() override {
    mock_storage_observer_ = std::make_unique<MockRemovableStorageObserver>();
    TestStorageMonitor* monitor = TestStorageMonitor::CreateAndInstall();
    mtp_device_observer_ = std::make_unique<FakeMtpManagerClientChromeOS>(
        monitor->receiver(), monitor->media_transfer_protocol_manager());
    monitor->AddObserver(mock_storage_observer_.get());
  }

  void TearDown() override {
    StorageMonitor* monitor = StorageMonitor::GetInstance();
    monitor->RemoveObserver(mock_storage_observer_.get());
    mtp_device_observer_.reset();
    TestStorageMonitor::Destroy();
  }

  // Returns the device changed observer object.
  MockRemovableStorageObserver& observer() { return *mock_storage_observer_; }

  FakeMtpManagerClientChromeOS* mtp_device_observer() {
    return mtp_device_observer_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<FakeMtpManagerClientChromeOS> mtp_device_observer_;
  std::unique_ptr<MockRemovableStorageObserver> mock_storage_observer_;
};

// Test to verify basic MTP storage attach and detach notifications.
TEST_F(MtpManagerClientChromeOSTest, BasicAttachDetach) {
  auto* mtpStorageInfo = GetFakeMtpStorageInfoSync(kStorageWithValidInfo);
  std::string device_id = GetDeviceIdFromStorageInfo(*mtpStorageInfo);

  // Attach a MTP storage.
  mtp_device_observer()->MtpStorageAttached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_attached().device_id());
  EXPECT_EQ(GetDeviceLocationFromStorageName(kStorageWithValidInfo),
            observer().last_attached().location());
  EXPECT_EQ(kStorageVendor16, observer().last_attached().vendor_name());
  EXPECT_EQ(kStorageProduct16, observer().last_attached().model_name());

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_detached().device_id());
}

// When a MTP storage device with invalid storage label and id is
// attached/detached, there should not be any device attach/detach
// notifications.
TEST_F(MtpManagerClientChromeOSTest, StorageWithInvalidInfo) {
  // Attach the mtp storage with invalid storage info.
  mtp_device_observer()->MtpStorageAttached(kStorageWithInvalidInfo);

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithInvalidInfo);

  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
}

}  // namespace storage_monitor
