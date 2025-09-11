// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auto_sign_out/auto_sign_out_service.h"

#include <memory>

#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& guid,
    const std::string& name,
    base::Time signin_timestamp) {
  return std::make_unique<syncer::DeviceInfo>(
      /*guid=*/guid,
      /*client_name=*/name,
      /*chrome_version=*/"chrome_version",
      /*sync_user_agent=*/"user_agent",
      /*device_type=*/sync_pb::SyncEnums::TYPE_UNSET,
      /*os_type=*/syncer::DeviceInfo::OsType::kUnknown,
      /*form_factor=*/syncer::DeviceInfo::FormFactor::kUnknown,
      /*signin_scoped_device_id=*/"device_id",
      /*manufacturer_name=*/"manufacturer_name",
      /*model_name=*/"model_name",
      /*full_hardware_class=*/"full_hardware_class",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*pulse_interval=*/base::Minutes(60),
      /*send_tab_to_self_receiving_enabled=*/false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/"token",
      /*interested_data_types=*/syncer::DataTypeSet(),
      /*floating_workspace_last_signin_timestamp=*/signin_timestamp);
}

}  // namespace

class AutoSignOutTest : public testing::Test {
 public:
  AutoSignOutTest() = default;
  ~AutoSignOutTest() override = default;

 protected:
  syncer::FakeDeviceInfoSyncService* fake_device_info_sync_service() {
    return fake_device_info_sync_service_.get();
  }

  syncer::TestSyncService* test_sync_service() { return &test_sync_service_; }

  session_manager::SessionManager* session_manager() {
    return session_manager_.get();
  }

  session_manager::FakeSessionManagerDelegate* fake_session_manager_delegate() {
    return fake_session_manager_delegate_.get();
  }

  const syncer::DeviceInfo* local_device_info() const {
    return local_device_info_;
  }

  void SetUp() override {
    fake_device_info_sync_service_ =
        std::make_unique<syncer::FakeDeviceInfoSyncService>();

    local_device_info_ = fake_device_info_sync_service()
                             ->GetLocalDeviceInfoProvider()
                             ->GetLocalDeviceInfo();

    // Ensure local_device is initialized with fake info.
    ASSERT_NE(nullptr, local_device_info());

    auto delegate =
        std::make_unique<session_manager::FakeSessionManagerDelegate>();

    // Since SessionManager takes unique ownership of the delegate upon
    // construction, we must assign a non-owning pointer to it beforehand to
    // access it in the tests.
    fake_session_manager_delegate_ = delegate.get();

    session_manager_ =
        std::make_unique<session_manager::SessionManager>(std::move(delegate));
  }

  void TearDown() override {
    fake_session_manager_delegate_ = nullptr;
    session_manager_.reset();
    local_device_info_ = nullptr;
    fake_device_info_sync_service_.reset();
  }

 private:
  syncer::TestSyncService test_sync_service_;

  std::unique_ptr<syncer::FakeDeviceInfoSyncService>
      fake_device_info_sync_service_;

  raw_ptr<const syncer::DeviceInfo> local_device_info_ = nullptr;

  raw_ptr<session_manager::FakeSessionManagerDelegate>
      fake_session_manager_delegate_ = nullptr;

  std::unique_ptr<session_manager::SessionManager> session_manager_;
};

// Verifies that local device timestamp is updated when service is created.
TEST_F(AutoSignOutTest, TimestampUpdatedAfterServiceCreation) {
  EXPECT_FALSE(local_device_info()
                   ->floating_workspace_last_signin_timestamp()
                   .has_value());

  AutoSignOutService auto_sign_out_service(
      fake_device_info_sync_service(), test_sync_service(), session_manager());

  EXPECT_TRUE(local_device_info()
                  ->floating_workspace_last_signin_timestamp()
                  .has_value());
}

// Verifies that a sign-out is triggered when a newer device signs in.
TEST_F(AutoSignOutTest, SignOutTriggeredForNewerDeviceSignIn) {
  AutoSignOutService auto_sign_out_service(
      fake_device_info_sync_service(), test_sync_service(), session_manager());

  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + base::Seconds(10)));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);

  ASSERT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 0);

  test_sync_service()->FireStateChanged();

  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 1);
}

// Verifies that a sign-out is not triggered when an older device is already
// signed in.
TEST_F(AutoSignOutTest, SignOutNotTriggeredForOlderDeviceSignIn) {
  AutoSignOutService auto_sign_out_service(
      fake_device_info_sync_service(), test_sync_service(), session_manager());

  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() - base::Seconds(10)));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);

  test_sync_service()->FireStateChanged();

  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 0);
}

// Verifies that a sign-out is not triggered when the same device signs in.
TEST_F(AutoSignOutTest, SignOutNotTriggeredWithSameDeviceInfoGuid) {
  AutoSignOutService auto_sign_out_service(
      fake_device_info_sync_service(), test_sync_service(), session_manager());

  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + base::Seconds(10)));

  fake_device_info_sync_service()->GetDeviceInfoTracker()->SetLocalCacheGuid(
      "guid1");

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);

  test_sync_service()->FireStateChanged();

  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 0);
}

}  // namespace ash
