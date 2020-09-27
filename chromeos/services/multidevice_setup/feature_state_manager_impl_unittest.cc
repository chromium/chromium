// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/fake_wifi_sync_feature_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

multidevice::RemoteDeviceRef CreateTestLocalDevice() {
  multidevice::RemoteDeviceRef local_device =
      multidevice::CreateRemoteDeviceRefForTest();

  // Set all client features to not supported.
  multidevice::RemoteDevice* raw_device =
      multidevice::GetMutableRemoteDevice(local_device);
  raw_device
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherClient] =
      multidevice::SoftwareFeatureState::kNotSupported;
  raw_device
      ->software_features[multidevice::SoftwareFeature::kSmartLockClient] =
      multidevice::SoftwareFeatureState::kNotSupported;
  raw_device->software_features
      [multidevice::SoftwareFeature::kInstantTetheringClient] =
      multidevice::SoftwareFeatureState::kNotSupported;
  raw_device
      ->software_features[multidevice::SoftwareFeature::kMessagesForWebClient] =
      multidevice::SoftwareFeatureState::kNotSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kPhoneHubClient] =
      multidevice::SoftwareFeatureState::kNotSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kWifiSyncClient] =
      multidevice::SoftwareFeatureState::kNotSupported;

  return local_device;
}

multidevice::RemoteDeviceRef CreateTestHostDevice() {
  multidevice::RemoteDeviceRef host_device =
      multidevice::CreateRemoteDeviceRefForTest();

  // Set all host features to supported.
  multidevice::RemoteDevice* raw_device =
      multidevice::GetMutableRemoteDevice(host_device);
  raw_device
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kSmartLockHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device
      ->software_features[multidevice::SoftwareFeature::kInstantTetheringHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device
      ->software_features[multidevice::SoftwareFeature::kMessagesForWebHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kPhoneHubHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kWifiSyncHost] =
      multidevice::SoftwareFeatureState::kSupported;

  return host_device;
}

}  // namespace

class MultiDeviceSetupFeatureStateManagerImplTest : public testing::Test {
 protected:
  MultiDeviceSetupFeatureStateManagerImplTest()
      : test_local_device_(CreateTestLocalDevice()),
        test_host_device_(CreateTestHostDevice()) {}
  ~MultiDeviceSetupFeatureStateManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    user_prefs::PrefRegistrySyncable* registry = test_pref_service_->registry();
    RegisterFeaturePrefs(registry);

    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(
        multidevice::RemoteDeviceRefList{test_local_device_,
                                         test_host_device_});
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);

    fake_android_sms_pairing_state_tracker_ =
        std::make_unique<FakeAndroidSmsPairingStateTracker>();
    fake_android_sms_pairing_state_tracker_->SetPairingComplete(true);

    fake_wifi_sync_feature_manager_ =
        std::make_unique<FakeWifiSyncFeatureManager>();

    manager_ = FeatureStateManagerImpl::Factory::Create(
        test_pref_service_.get(), fake_host_status_provider_.get(),
        fake_device_sync_client_.get(),
        fake_android_sms_pairing_state_tracker_.get(),
        fake_wifi_sync_feature_manager_.get());

    fake_observer_ = std::make_unique<FakeFeatureStateManagerObserver>();
    manager_->AddObserver(fake_observer_.get());
  }

  void TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature feature) {
    bool was_previously_verified =
        fake_host_status_provider_->GetHostWithStatus().host_status() ==
        mojom::HostStatus::kHostVerified;
    size_t num_observer_events_before_call =
        fake_observer_->feature_state_updates().size();
    size_t expected_num_observer_events_after_call =
        num_observer_events_before_call + (was_previously_verified ? 1u : 0u);

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kNoEligibleHosts, base::nullopt /* host_device */);
    if (was_previously_verified) {
      VerifyFeatureStateChange(num_observer_events_before_call, feature,
                               mojom::FeatureState::kUnavailableNoVerifiedHost);
    }
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        base::nullopt /* host_device */);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
        test_host_device_);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetButNotYetVerified, test_host_device_);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());
  }

  void SetVerifiedHost() {
    // Should not already be verified if we are setting it to be verified.
    EXPECT_NE(mojom::HostStatus::kHostVerified,
              fake_host_status_provider_->GetHostWithStatus().host_status());

    size_t num_observer_events_before_call =
        fake_observer_->feature_state_updates().size();

    SetSoftwareFeatureState(false /* use_local_device */,
                            multidevice::SoftwareFeature::kBetterTogetherHost,
                            multidevice::SoftwareFeatureState::kEnabled);
    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostVerified, test_host_device_);

    // Since the host is now verified, there should be a feature state update
    // for all features.
    EXPECT_EQ(num_observer_events_before_call + 1u,
              fake_observer_->feature_state_updates().size());
  }

  void MakeBetterTogetherSuiteDisabledByUser() {
    SetSoftwareFeatureState(true /* use_local_device */,
                            multidevice::SoftwareFeature::kBetterTogetherClient,
                            multidevice::SoftwareFeatureState::kSupported);
    test_pref_service_->SetBoolean(kBetterTogetherSuiteEnabledPrefName, false);
    EXPECT_EQ(
        mojom::FeatureState::kDisabledByUser,
        manager_->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  }

  void VerifyFeatureState(mojom::FeatureState expected_feature_state,
                          mojom::Feature feature) {
    EXPECT_TRUE(base::Contains(manager_->GetFeatureStates(), feature));
    EXPECT_EQ(expected_feature_state, manager_->GetFeatureStates()[feature]);
  }

  void VerifyFeatureStateChange(size_t expected_index,
                                mojom::Feature expected_feature,
                                mojom::FeatureState expected_feature_state) {
    const FeatureStateManager::FeatureStatesMap& map =
        fake_observer_->feature_state_updates()[expected_index];
    const auto it = map.find(expected_feature);
    EXPECT_NE(map.end(), it);
    EXPECT_EQ(expected_feature_state, it->second);
  }

  void SetSoftwareFeatureState(
      bool use_local_device,
      multidevice::SoftwareFeature software_feature,
      multidevice::SoftwareFeatureState software_feature_state) {
    multidevice::RemoteDeviceRef& device =
        use_local_device ? test_local_device_ : test_host_device_;
    multidevice::GetMutableRemoteDevice(device)
        ->software_features[software_feature] = software_feature_state;
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void SetAndroidSmsPairingState(bool is_paired) {
    fake_android_sms_pairing_state_tracker_->SetPairingComplete(is_paired);
  }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return test_pref_service_.get();
  }

  FeatureStateManager* manager() { return manager_.get(); }
  FakeWifiSyncFeatureManager* wifi_sync_manager() {
    return fake_wifi_sync_feature_manager_.get();
  }

 private:
  multidevice::RemoteDeviceRef test_local_device_;
  multidevice::RemoteDeviceRef test_host_device_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeAndroidSmsPairingStateTracker>
      fake_android_sms_pairing_state_tracker_;
  std::unique_ptr<FakeWifiSyncFeatureManager> fake_wifi_sync_feature_manager_;

  std::unique_ptr<FakeFeatureStateManagerObserver> fake_observer_;

  std::unique_ptr<FeatureStateManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupFeatureStateManagerImplTest);
};

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, BetterTogetherSuite) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(
      mojom::Feature::kBetterTogetherSuite);

  SetVerifiedHost();
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kBetterTogetherSuite);

  // Add support for the suite; it should still remain unsupported, since there
  // are no sub-features which are supported.
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kBetterTogetherClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kBetterTogetherSuite);

  // Add support for child features.
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kInstantTetheringClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kSmartLockClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kMessagesForWebClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kWifiSyncClient,
                          multidevice::SoftwareFeatureState::kSupported);

  // Now, the suite should be considered enabled.
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureStateChange(5u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kEnabledByUser);

  test_pref_service()->SetBoolean(kBetterTogetherSuiteEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureStateChange(6u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kDisabledByUser);

  // Set all features to prohibited. This should cause the Better Together suite
  // to become prohibited as well.
  test_pref_service()->SetBoolean(kInstantTetheringAllowedPrefName, false);
  test_pref_service()->SetBoolean(kMessagesAllowedPrefName, false);
  test_pref_service()->SetBoolean(kSmartLockAllowedPrefName, false);
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  test_pref_service()->SetBoolean(kWifiSyncAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureStateChange(11u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, InstantTethering) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(
      mojom::Feature::kInstantTethering);

  SetVerifiedHost();
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kInstantTethering);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kInstantTetheringClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(1u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kInstantTetheringHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(2u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(4u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kInstantTetheringEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(5u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kInstantTetheringAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(6u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, Messages) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kMessages);

  SetVerifiedHost();
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kMessages);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kMessagesForWebClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kMessages);
  VerifyFeatureStateChange(1u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kMessagesForWebHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kMessages);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kEnabledByUser);

  SetAndroidSmsPairingState(false /* is_paired */);
  VerifyFeatureState(mojom::FeatureState::kFurtherSetupRequired,
                     mojom::Feature::kMessages);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kFurtherSetupRequired);

  SetAndroidSmsPairingState(true /* is_paired */);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kMessages);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kEnabledByUser);

  SetAndroidSmsPairingState(false /* is_paired */);
  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kMessages);
  VerifyFeatureStateChange(7u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  SetAndroidSmsPairingState(true /* is_paired */);
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kMessages);

  test_pref_service()->SetBoolean(kMessagesEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kMessages);
  VerifyFeatureStateChange(8u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kMessagesAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kMessages);
  VerifyFeatureStateChange(9u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, SmartLock) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kSmartLock);

  SetVerifiedHost();
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kSmartLock);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kSmartLockClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kUnavailableInsufficientSecurity,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(
      1u /* expected_index */, mojom::Feature::kSmartLock,
      mojom::FeatureState::kUnavailableInsufficientSecurity);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kSmartLockHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kSmartLockEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(5u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kSmartLockAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(6u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, PhoneHub) {
  const std::vector<mojom::Feature> kAllPhoneHubFeatures{
      mojom::Feature::kPhoneHub, mojom::Feature::kPhoneHubNotifications,
      mojom::Feature::kPhoneHubNotificationBadge,
      mojom::Feature::kPhoneHubTaskContinuation};

  for (const auto& phone_hub_feature : kAllPhoneHubFeatures)
    TryAllUnverifiedHostStatesAndVerifyFeatureState(phone_hub_feature);

  SetVerifiedHost();
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                       phone_hub_feature);
  }

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(1u /* expected_index */, mojom::Feature::kPhoneHub,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kEnabledByUser, phone_hub_feature);
  }
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kPhoneHub,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kPhoneHub,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  // Disabling Phone Hub notifications implicitly makes the notification badge
  // unavailable.
  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kPhoneHubNotificationBadge);
  VerifyFeatureStateChange(5u /* expected_index */,
                           mojom::Feature::kPhoneHubNotifications,
                           mojom::FeatureState::kDisabledByUser);

  // Re-enable Phone Hub notifications, then disable Phone Hub, which implicitly
  // implicitly makes all of its sub-features unavailable.
  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, true);
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kPhoneHubNotificationBadge);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kPhoneHubTaskContinuation);
  VerifyFeatureStateChange(7u /* expected_index */, mojom::Feature::kPhoneHub,
                           mojom::FeatureState::kDisabledByUser);

  // Prohibiting Phone Hub notifications implicitly prohibits the notification
  // badge.
  test_pref_service()->SetBoolean(kPhoneHubNotificationsAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kPhoneHubNotificationBadge);
  VerifyFeatureStateChange(8u /* expected_index */,
                           mojom::Feature::kPhoneHubNotifications,
                           mojom::FeatureState::kProhibitedByPolicy);

  // Prohibiting Phone Hub implicitly prohibits all of its sub-features.
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(9u /* expected_index */, mojom::Feature::kPhoneHub,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, WifiSync) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kWifiSync);

  SetVerifiedHost();
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kWifiSync);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kWifiSyncClient,
                          multidevice::SoftwareFeatureState::kSupported);
  wifi_sync_manager()->SetIsWifiSyncEnabled(true);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(1u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kDisabledByUser);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kWifiSyncHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  wifi_sync_manager()->SetIsWifiSyncEnabled(false);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kEnabledByUser);

  manager()->SetFeatureEnabledState(mojom::Feature::kWifiSync, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kDisabledByUser);

  manager()->SetFeatureEnabledState(mojom::Feature::kWifiSync, true);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(6u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kWifiSyncAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(7u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kProhibitedByPolicy);
}

}  // namespace multidevice_setup

}  // namespace chromeos
