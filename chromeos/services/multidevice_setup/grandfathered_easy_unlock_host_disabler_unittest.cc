// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"

#include <memory>

#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const char kEasyUnlockHostIdToDisablePrefName[] =
    "multidevice_setup.easy_unlock_host_id_to_disable";

const char kNoDevice[] = "";

const size_t kNumTestDevices = 2;

}  // namespace

class MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest
    : public testing::Test {
 protected:
  MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest()
      : test_devices_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_host_backend_delegate_ = std::make_unique<FakeHostBackendDelegate>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    GrandfatheredEasyUnlockHostDisabler::RegisterPrefs(
        test_pref_service_->registry());
  }

  void SetHost(const base::Optional<cryptauth::RemoteDeviceRef>& host_device,
               cryptauth::SoftwareFeature host_type) {
    if (host_type != cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST &&
        host_type != cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)
      return;

    for (const auto& remote_device : test_devices_) {
      bool should_be_host =
          host_device != base::nullopt &&
          host_device->GetDeviceId() == remote_device.GetDeviceId();

      GetMutableRemoteDevice(remote_device)->software_features[host_type] =
          should_be_host ? cryptauth::SoftwareFeatureState::kEnabled
                         : cryptauth::SoftwareFeatureState::kSupported;
    }

    if (host_type == cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST)
      fake_host_backend_delegate_->NotifyHostChangedOnBackend(host_device);
  }

  void InitializeTest(
      const std::string& initial_device_id_pref_value,
      base::Optional<cryptauth::RemoteDeviceRef> initial_better_together_host,
      base::Optional<cryptauth::RemoteDeviceRef> initial_easy_unlock_host) {
    test_pref_service_->SetString(kEasyUnlockHostIdToDisablePrefName,
                                  initial_device_id_pref_value);

    SetHost(initial_better_together_host,
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
    SetHost(initial_easy_unlock_host,
            cryptauth::SoftwareFeature::EASY_UNLOCK_HOST);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    grandfathered_easy_unlock_host_disabler_ =
        GrandfatheredEasyUnlockHostDisabler::Factory::Get()->BuildInstance(
            fake_host_backend_delegate_.get(), fake_device_sync_client_.get(),
            test_pref_service_.get(), std::move(mock_timer));
  }

  std::string GetEasyUnlockHostIdToDisablePrefValue() {
    return test_pref_service_->GetString(kEasyUnlockHostIdToDisablePrefName);
  }

  const cryptauth::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() const {
    return fake_device_sync_client_.get();
  }

  base::MockOneShotTimer* mock_timer() const { return mock_timer_; }

 private:
  cryptauth::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeHostBackendDelegate> fake_host_backend_delegate_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  base::MockOneShotTimer* mock_timer_ = nullptr;

  std::unique_ptr<GrandfatheredEasyUnlockHostDisabler>
      grandfathered_easy_unlock_host_disabler_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest);
};

// Situation #1:
//   BTH = BETTER_TOGETHER_HOST, 0 = disabled, A = devices[0]
//   EUH = EASY_UNLOCK_HOST,     1 = enabled,  B = devices[1]
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 0 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 1 | 0 |
//
// Grandfathering prevents EUH from being disabled automatically. This class
// disables EUH manually.
TEST_F(
    MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
    IfBetterTogetherHostChangedFromOneDeviceToNoDeviceThenDisableEasyUnlock) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(base::nullopt, cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(test_devices()[0].GetDeviceId(),
            GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      1,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());

  fake_device_sync_client()->InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess);

  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_FALSE(mock_timer()->IsRunning());
}

// Situation #2:
//   BTH = BETTER_TOGETHER_HOST, 0 = disabled, A = devices[0]
//   EUH = EASY_UNLOCK_HOST,     1 = enabled,  B = devices[1]
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 0 | 0 |        BTH| 0 | 1 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 0 | 1 |
//
// The CryptAuth backend (via GmsCore) disables EUH on device A when BTH is
// enabled (exclusively) on another device, B. No action necessary from this
// class.
TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfBetterTogetherHostChangedFromNoDeviceToADeviceThenDoNothing) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 base::nullopt /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(test_devices()[1], cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      0,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
}

// Situation #3:
//   BTH = BETTER_TOGETHER_HOST, 0 = disabled, A = devices[0]
//   EUH = EASY_UNLOCK_HOST,     1 = enabled,  B = devices[1]
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 1 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 0 | 1 |
//
// The CryptAuth backend (via GmsCore) disables EUH on device A when BTH is
// enabled (exclusively) on another device, B. We still attempt to disable
// EUH in this case to be safe.
TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfBetterTogetherHostChangedFromOneDeviceToAnotherThenDisableEasyUnlock) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(test_devices()[1], cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(test_devices()[0].GetDeviceId(),
            GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      1,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());

  fake_device_sync_client()->InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess);

  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDisablePendingThenConstructorAttemptsToDisableEasyUnlock) {
  InitializeTest(
      test_devices()[0].GetDeviceId() /* initial_device_id_pref_value */,
      base::nullopt /* initial_better_together_host */,
      test_devices()[0] /* initial_easy_unlock_host */);

  EXPECT_EQ(test_devices()[0].GetDeviceId(),
            GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      1,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
}

// Situation #1 where device A is removed from list of synced devices:
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 0 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 1 | 0 |
TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfHostToDisableIsNotInListOfSyncedDevicesThenClearPref) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  // Remove device[0] from list
  fake_device_sync_client()->set_synced_devices({test_devices()[1]});

  SetHost(base::nullopt, cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      0,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
}

// Situation #1 with failure:
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 0 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 1 | 0 |
TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfEasyUnlockDisableUnsuccessfulThenScheduleRetry) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(base::nullopt, cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(
      1,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
  fake_device_sync_client()->InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kInternalServerError);
  EXPECT_EQ(
      0,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());

  EXPECT_EQ(test_devices()[0].GetDeviceId(),
            GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_TRUE(mock_timer()->IsRunning());

  mock_timer()->Fire();

  EXPECT_EQ(
      1,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
}

TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfNoDisablePendingThenConstructorDoesNothing) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 base::nullopt /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      0,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
}

TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDisablePendingButIsNotCurrentEasyUnlockHostThenClearPref) {
  InitializeTest(
      test_devices()[0].GetDeviceId() /* initial_device_id_pref_value */,
      test_devices()[1] /* initial_better_together_host */,
      test_devices()[1] /* initial_easy_unlock_host */);

  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      0,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
}

TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDisablePendingButIsCurrentBetterTogetherHostThenClearPref) {
  InitializeTest(
      test_devices()[0].GetDeviceId() /* initial_device_id_pref_value */,
      test_devices()[0] /* initial_better_together_host */,
      test_devices()[0] /* initial_easy_unlock_host */);

  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
  EXPECT_EQ(
      0,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
}

// Simulate:
//   - Disable BETTER_TOGETHER_HOST on device 0
//   - GrandfatheredEasyUnlockHostDisabler tries to disable EASY_UNLOCK_HOST on
//     device 0 but fails
//   - Timer is running while we wait to retry
//   - Re-enable BETTER_TOGETHER_HOST on device 0
TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfHostChangesWhileRetryTimerIsRunningThenCancelTimerAndClearPref) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(base::nullopt, cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(
      1,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
  fake_device_sync_client()->InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kInternalServerError);

  EXPECT_TRUE(mock_timer()->IsRunning());

  SetHost(test_devices()[0], cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(
      0,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
  EXPECT_FALSE(mock_timer()->IsRunning());
  EXPECT_EQ(kNoDevice, GetEasyUnlockHostIdToDisablePrefValue());
}

// Simulate:
//   - Set device 0 as host
//   - Disable host
//   - Set device 1 as host
//   - Disable host
//   - SetSoftwareFeatureState callback for device 0 is called
TEST_F(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDifferentHostDisabledBeforeFirstCallbackThenFirstCallbackDoesNothing) {
  InitializeTest(kNoDevice /* initial_device_id_pref_value */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(base::nullopt, cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  SetHost(test_devices()[1], cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
  SetHost(test_devices()[1], cryptauth::SoftwareFeature::EASY_UNLOCK_HOST);

  SetHost(base::nullopt, cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);

  EXPECT_EQ(
      2,
      fake_device_sync_client()->GetSetSoftwareFeatureStateCallbackQueueSize());
  EXPECT_EQ(test_devices()[1].GetDeviceId(),
            GetEasyUnlockHostIdToDisablePrefValue());

  fake_device_sync_client()->InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess);
  EXPECT_EQ(test_devices()[1].GetDeviceId(),
            GetEasyUnlockHostIdToDisablePrefValue());
}

}  // namespace multidevice_setup

}  // namespace chromeos
