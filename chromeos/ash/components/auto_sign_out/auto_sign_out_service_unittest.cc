// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auto_sign_out/auto_sign_out_service.h"

#include <memory>

#include "chromeos/constants/pref_names.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "content/public/test/browser_task_environment.h"
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
      /*auto_sign_out_last_signin_timestamp=*/signin_timestamp);
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

  TestingPrefServiceSimple* prefs() { return &prefs_; }

  session_manager::FakeSessionManagerDelegate* fake_session_manager_delegate() {
    return fake_session_manager_delegate_.get();
  }

  chromeos::FakePowerManagerClient* fake_power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  const syncer::DeviceInfo* local_device_info() const {
    return local_device_info_;
  }

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

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

    // Set `kAutoSignOutEnabled` as true by default since we need it enabled for
    // most tests.
    prefs_.registry()->RegisterBooleanPref(chromeos::prefs::kAutoSignOutEnabled,
                                           true);
    prefs_.registry()->RegisterBooleanPref(chromeos::prefs::kFloatingSsoEnabled,
                                           false);
    prefs_.registry()->RegisterBooleanPref(
        chromeos::prefs::kFloatingWorkspaceV2Enabled, false);
  }

  void TearDown() override {
    fake_session_manager_delegate_ = nullptr;
    session_manager_.reset();
    local_device_info_ = nullptr;
    fake_device_info_sync_service_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  syncer::TestSyncService test_sync_service_;

  TestingPrefServiceSimple prefs_;

  std::unique_ptr<syncer::FakeDeviceInfoSyncService>
      fake_device_info_sync_service_;

  raw_ptr<const syncer::DeviceInfo> local_device_info_ = nullptr;

  raw_ptr<session_manager::FakeSessionManagerDelegate>
      fake_session_manager_delegate_ = nullptr;

  std::unique_ptr<session_manager::SessionManager> session_manager_;
};

// Verifies that local device info timestamp is updated when service is created.
TEST_F(AutoSignOutTest, TimestampUpdatedAfterServiceCreation) {
  EXPECT_FALSE(
      local_device_info()->auto_sign_out_last_signin_timestamp().has_value());

  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

  EXPECT_TRUE(
      local_device_info()->auto_sign_out_last_signin_timestamp().has_value());
}

// Verifies that a sign-out is triggered when a newer device signs in.
TEST_F(AutoSignOutTest, SignOutTriggeredForNewerDeviceSignIn) {
  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

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
  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

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
  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

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

// Verifies that device info timestamp is updated on unlock after wake-up from
// sleep.
TEST_F(AutoSignOutTest, TimestampUpdatedOnUnlockAfterWakeUpFromSleep) {
  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

  const base::Time timestamp_before_sleep =
      local_device_info()->auto_sign_out_last_signin_timestamp().value();

  // Simulate sleep.
  fake_power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  task_environment().FastForwardBy(base::Seconds(10));

  // Simulate waking up.
  fake_power_manager_client()->SendSuspendDone();

  const base::Time timestamp_after_wakeup =
      local_device_info()->auto_sign_out_last_signin_timestamp().value();

  EXPECT_EQ(timestamp_after_wakeup, timestamp_before_sleep);

  session_manager::SessionManager::Get()->NotifyUnlockAttempt(
      /*success=*/true, /*UnlockType=*/session_manager::UnlockType::PASSWORD);

  const base::Time timestamp_after_unlock =
      local_device_info()->auto_sign_out_last_signin_timestamp().value();

  EXPECT_GT(timestamp_after_unlock, timestamp_after_wakeup);
}

// Verifies that device info timestamp is not updated on unlock if device wasn't
// previously asleep.
TEST_F(AutoSignOutTest, TimestampNotUpdatedOnUnlockWithoutPreviousSleep) {
  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

  const base::Time timestamp_before_unlock =
      local_device_info()->auto_sign_out_last_signin_timestamp().value();

  task_environment().FastForwardBy(base::Seconds(10));

  session_manager::SessionManager::Get()->NotifyUnlockAttempt(
      /*success=*/true, /*UnlockType=*/session_manager::UnlockType::PASSWORD);

  const base::Time timestamp_after_unlock =
      local_device_info()->auto_sign_out_last_signin_timestamp().value();

  EXPECT_EQ(timestamp_after_unlock, timestamp_before_unlock);
}

// Verifies that receiving new device info immediately after waking up doesn't
// trigger sign-out.
TEST_F(AutoSignOutTest, SignOutNotTriggeredOnWakeUp) {
  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

  // Simulate sleep.
  fake_power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  // Simulate another device being active while the first device is asleep.
  constexpr base::TimeDelta new_device_timestamp_delta = base::Seconds(10);
  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + new_device_timestamp_delta));

  constexpr base::TimeDelta sleep_duration =
      new_device_timestamp_delta + base::Seconds(5);

  // Sleep past the activity timestamp of the other device.
  task_environment().FastForwardBy(sleep_duration);

  // Simulate waking up.
  fake_power_manager_client()->SendSuspendDone();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);

  test_sync_service()->FireStateChanged();

  // Verifies sign-out is not requested since our device has woken up after the
  // sign-in timestamp of the other device.
  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 0);
}

// Verifies that toggling the `kAutoSignOutEnabled` pref correctly handles the
// sign-out request when a newer device signs in.
TEST_F(AutoSignOutTest, ServiceReactsToPrefChangesForNewerDeviceSignIn) {
  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

  // Newer device signs in for the first time.
  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + base::Seconds(10)));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);

  ASSERT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 0);

  test_sync_service()->FireStateChanged();

  // Sign-out count increased since pref is enabled.
  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 1);

  prefs()->SetBoolean(chromeos::prefs::kAutoSignOutEnabled, false);

  // Newer device signs in for the second time.
  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + base::Seconds(10)));

  test_sync_service()->FireStateChanged();

  // Sign-out count remained the same since pref is disabled.
  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 1);

  prefs()->SetBoolean(chromeos::prefs::kAutoSignOutEnabled, true);

  test_sync_service()->FireStateChanged();

  // Sign-out count increased since pref is enabled.
  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 2);
}

namespace {

struct TestConfig {
  bool auto_sign_out_enabled = false;
  bool floating_sso_enabled = false;
  bool floating_workspace_v2_enabled = false;
  int expected_sign_out_count = 0;
};

}  // namespace

// Test fixture that is used to test various pref combinations.
class AutoSignOutPrefTest : public AutoSignOutTest,
                            public testing::WithParamInterface<TestConfig> {};

// Verifies that a sign-out is triggered for a newer device sign-in when any of
// the policies is enabled.
TEST_P(AutoSignOutPrefTest,
       SignOutTriggeredForNewerDeviceSignInWhenAnyPrefIsEnabled) {
  const TestConfig& test_config = GetParam();

  prefs()->SetBoolean(chromeos::prefs::kAutoSignOutEnabled,
                      test_config.auto_sign_out_enabled);
  prefs()->SetBoolean(chromeos::prefs::kFloatingSsoEnabled,
                      test_config.floating_sso_enabled);
  prefs()->SetBoolean(chromeos::prefs::kFloatingWorkspaceV2Enabled,
                      test_config.floating_workspace_v2_enabled);

  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service(),
                                           test_sync_service(),
                                           session_manager(), prefs());

  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + base::Seconds(10)));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);

  ASSERT_EQ(fake_session_manager_delegate()->request_sign_out_count(), 0);

  test_sync_service()->FireStateChanged();

  EXPECT_EQ(fake_session_manager_delegate()->request_sign_out_count(),
            test_config.expected_sign_out_count);
}

INSTANTIATE_TEST_SUITE_P(
    PrefCombinations,
    AutoSignOutPrefTest,
    ::testing::Values(
        TestConfig{},
        TestConfig{.auto_sign_out_enabled = true, .expected_sign_out_count = 1},
        TestConfig{.floating_sso_enabled = true, .expected_sign_out_count = 1},
        TestConfig{.floating_workspace_v2_enabled = true,
                   .expected_sign_out_count = 1},
        TestConfig{.auto_sign_out_enabled = true,
                   .floating_sso_enabled = true,
                   .floating_workspace_v2_enabled = true,
                   .expected_sign_out_count = 1}));

}  // namespace ash
