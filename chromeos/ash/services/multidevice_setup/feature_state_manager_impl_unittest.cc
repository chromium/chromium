// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/feature_state_manager_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

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
  raw_device->software_features[multidevice::SoftwareFeature::kPhoneHubClient] =
      multidevice::SoftwareFeatureState::kNotSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kWifiSyncClient] =
      multidevice::SoftwareFeatureState::kNotSupported;

  return local_device;
}

multidevice::RemoteDeviceRef CreateTestHostDevice(
    bool empty_mac_address = false) {
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
  raw_device->software_features[multidevice::SoftwareFeature::kPhoneHubHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kWifiSyncHost] =
      multidevice::SoftwareFeatureState::kSupported;

  if (empty_mac_address) {
    raw_device->bluetooth_public_address.clear();
  }

  return host_device;
}

}  // namespace

class MultiDeviceSetupFeatureStateManagerImplTest : public testing::Test {
 public:
  MultiDeviceSetupFeatureStateManagerImplTest(
      const MultiDeviceSetupFeatureStateManagerImplTest&) = delete;
  MultiDeviceSetupFeatureStateManagerImplTest& operator=(
      const MultiDeviceSetupFeatureStateManagerImplTest&) = delete;

 protected:
  MultiDeviceSetupFeatureStateManagerImplTest()
      : test_local_device_(CreateTestLocalDevice()),
        test_host_device_(CreateTestHostDevice()) {}
  ~MultiDeviceSetupFeatureStateManagerImplTest() override = default;

  void SetupFeatureStateManager(bool is_secondary_user = false,
                                bool empty_mac_address = false) {
    test_host_device_ = CreateTestHostDevice(empty_mac_address);
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

    fake_global_state_feature_managers_.emplace(
        mojom::Feature::kWifiSync,
        std::make_unique<FakeGlobalStateFeatureManager>());

    manager_ = FeatureStateManagerImpl::Factory::Create(
        test_pref_service_.get(), fake_host_status_provider_.get(),
        fake_device_sync_client_.get(),
        fake_android_sms_pairing_state_tracker_.get(),
        {{mojom::Feature::kWifiSync,
          fake_global_state_feature_managers_.at(mojom::Feature::kWifiSync)
              .get()}},
        is_secondary_user);

    fake_observer_ = std::make_unique<FakeFeatureStateManagerObserver>();
    manager_->AddObserver(fake_observer_.get());
  }

  void TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature feature) {
    bool previously_had_eligible_hosts =
        fake_host_status_provider_->GetHostWithStatus().host_status() !=
        mojom::HostStatus::kNoEligibleHosts;
    size_t num_observer_events_before_call =
        fake_observer_->feature_state_updates().size();
    size_t expected_num_observer_events_after_call =
        num_observer_events_before_call +
        (previously_had_eligible_hosts ? 1u : 0u);

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kNoEligibleHosts, std::nullopt /* host_device */);
    if (previously_had_eligible_hosts) {
      VerifyFeatureStateChange(
          num_observer_events_before_call, feature,
          mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts);
    }
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        std::nullopt /* host_device */);
    // Updating host status here updates feature state from
    // kUnavailableNoVerifiedHost_NoEligibleHosts to
    // kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified.
    expected_num_observer_events_after_call += 1;
    EXPECT_EQ(mojom::FeatureState::
                  kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
        test_host_device_);
    EXPECT_EQ(mojom::FeatureState::
                  kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetButNotYetVerified, test_host_device_);
    EXPECT_EQ(mojom::FeatureState::
                  kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
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

  void MakeBetterTogetherSuiteDisabledByUser(
      const mojom::FeatureState& expected_state_upon_disabling =
          mojom::FeatureState::kDisabledByUser) {
    SetSoftwareFeatureState(true /* use_local_device */,
                            multidevice::SoftwareFeature::kBetterTogetherClient,
                            multidevice::SoftwareFeatureState::kSupported);
    test_pref_service_->SetBoolean(kBetterTogetherSuiteEnabledPrefName, false);

    EXPECT_EQ(
        expected_state_upon_disabling,
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

  base::flat_map<mojom::Feature,
                 std::unique_ptr<FakeGlobalStateFeatureManager>>&
  global_state_feature_managers() {
    return fake_global_state_feature_managers_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  multidevice::RemoteDeviceRef test_local_device_;
  multidevice::RemoteDeviceRef test_host_device_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeAndroidSmsPairingStateTracker>
      fake_android_sms_pairing_state_tracker_;
  base::flat_map<mojom::Feature, std::unique_ptr<FakeGlobalStateFeatureManager>>
      fake_global_state_feature_managers_;

  std::unique_ptr<FakeFeatureStateManagerObserver> fake_observer_;

  std::unique_ptr<FeatureStateManager> manager_;
};

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest,
       DisableAllowCrossDeviceFeatureSuiteFlag) {
  // Disable the "AllowCrossDeviceFeatureSuite" flag, which should force all
  // Cross Device features to be unsupported.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kAllowCrossDeviceFeatureSuite);

  // Enable all features
  SetupFeatureStateManager();
  SetVerifiedHost();
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kBetterTogetherClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kInstantTetheringClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kSmartLockClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kWifiSyncClient,
                          multidevice::SoftwareFeatureState::kSupported);

  // Verify that all features are unsupported.
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kSmartLock);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kPhoneHubTaskContinuation);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kWifiSync);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kEche);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kPhoneHubCameraRoll);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, BetterTogetherSuite) {
  SetupFeatureStateManager();

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
  test_pref_service()->SetBoolean(kSmartLockAllowedPrefName, false);
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  test_pref_service()->SetBoolean(kWifiSyncAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureStateChange(10u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest,
       BetterTogetherSuiteForSecondaryUsers) {
  SetupFeatureStateManager(/*is_secondary_user=*/true);

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
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kWifiSyncClient,
                          multidevice::SoftwareFeatureState::kSupported);

  // Now, the suite should be considered enabled.
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureStateChange(4u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kEnabledByUser);

  test_pref_service()->SetBoolean(kBetterTogetherSuiteEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureStateChange(5u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kDisabledByUser);

  // Set all features to prohibited. This should cause the Better Together suite
  // to become prohibited as well.
  test_pref_service()->SetBoolean(kInstantTetheringAllowedPrefName, false);
  test_pref_service()->SetBoolean(kSmartLockAllowedPrefName, false);
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  test_pref_service()->SetBoolean(kWifiSyncAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kBetterTogetherSuite);
  VerifyFeatureStateChange(9u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, InstantTethering) {
  SetupFeatureStateManager();
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
  VerifyFeatureStateChange(2u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kInstantTetheringHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(3u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(5u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kInstantTetheringEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(6u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kInstantTetheringAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kInstantTethering);
  VerifyFeatureStateChange(7u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, SmartLock) {
  SetupFeatureStateManager();

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
      2u /* expected_index */, mojom::Feature::kSmartLock,
      mojom::FeatureState::kUnavailableInsufficientSecurity);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kSmartLockHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(5u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kSmartLockEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(6u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kSmartLockAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kSmartLock);
  VerifyFeatureStateChange(7u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest,
       PhoneHubBluetoothAddressNotSynced) {
  SetupFeatureStateManager(/*is_secondary_user=*/false,
                           /*empty_mac_address=*/true);

  const std::vector<mojom::Feature> kAllPhoneHubFeatures{
      mojom::Feature::kPhoneHub, mojom::Feature::kPhoneHubCameraRoll,
      mojom::Feature::kPhoneHubNotifications,
      mojom::Feature::kPhoneHubTaskContinuation, mojom::Feature::kEche};

  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    TryAllUnverifiedHostStatesAndVerifyFeatureState(phone_hub_feature);
  }

  SetVerifiedHost();
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                       phone_hub_feature);
  }

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheClient,
                          multidevice::SoftwareFeatureState::kSupported);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                       phone_hub_feature);
  }

  // This pref is disabled for existing Better Together users; they must go to
  // settings to explicitly enable PhoneHub.
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, true);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheHost,
                          multidevice::SoftwareFeatureState::kSupported);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(kAllPhoneHubFeatures.size() * 2 /* expected_index */,
                           mojom::Feature::kPhoneHub,
                           mojom::FeatureState::kNotSupportedByPhone);

  MakeBetterTogetherSuiteDisabledByUser(
      /*expected_state_upon_disabling=*/mojom::FeatureState::kDisabledByUser);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 1 /* expected_index */,
      mojom::Feature::kPhoneHub, mojom::FeatureState::kNotSupportedByPhone);

  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kPhoneHubNotifications);

  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, true);
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, false);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                       phone_hub_feature);
  }

  test_pref_service()->SetBoolean(kPhoneHubNotificationsAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 2 /* expected_index */,
      mojom::Feature::kPhoneHubNotifications,
      mojom::FeatureState::kNotSupportedByPhone);

  // Prohibiting Phone Hub implicitly prohibits all of its sub-features.
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 3 /* expected_index */,
      mojom::Feature::kPhoneHub, mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, PhoneHubForSecondaryUsers) {
  SetupFeatureStateManager(/*is_secondary_user=*/true);

  const std::vector<mojom::Feature> kAllPhoneHubFeatures{
      mojom::Feature::kPhoneHub, mojom::Feature::kPhoneHubCameraRoll,
      mojom::Feature::kPhoneHubNotifications,
      mojom::Feature::kPhoneHubTaskContinuation, mojom::Feature::kEche};

  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    TryAllUnverifiedHostStatesAndVerifyFeatureState(phone_hub_feature);
  }

  SetVerifiedHost();
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                       phone_hub_feature);
  }

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheClient,
                          multidevice::SoftwareFeatureState::kSupported);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                       phone_hub_feature);
  }

  // This pref should be disabled for existing Better Together users;
  // they must go to settings to explicitly enable PhoneHub.
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, true);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                       phone_hub_feature);
  }

  MakeBetterTogetherSuiteDisabledByUser(
      /*expected_state_upon_disabling=*/mojom::FeatureState::
          kNotSupportedByChromebook);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                       phone_hub_feature);
  }

  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kPhoneHubNotifications);

  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, true);
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, false);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                       phone_hub_feature);
  }

  test_pref_service()->SetBoolean(kPhoneHubNotificationsAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureStateChange(kAllPhoneHubFeatures.size() * 2 /* expected_index */,
                           mojom::Feature::kPhoneHubNotifications,
                           mojom::FeatureState::kProhibitedByPolicy);

  // Prohibiting Phone Hub implicitly prohibits all of its sub-features.
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 1 /* expected_index */,
      mojom::Feature::kPhoneHub, mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, PhoneHub) {
  SetupFeatureStateManager();

  const std::vector<mojom::Feature> kAllPhoneHubFeatures{
      mojom::Feature::kPhoneHub, mojom::Feature::kPhoneHubNotifications,
      mojom::Feature::kPhoneHubTaskContinuation};

  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    TryAllUnverifiedHostStatesAndVerifyFeatureState(phone_hub_feature);
  }

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
  VerifyFeatureStateChange(kAllPhoneHubFeatures.size() * 2 /* expected_index */,
                           mojom::Feature::kPhoneHub,
                           mojom::FeatureState::kNotSupportedByPhone);

  // The top-level Phone Hub enabled pref is disabled for existing Better
  // Together users; they must go to settings to explicitly enable PhoneHub.
  // Likewise, the Phone Hub notifications enabled pref is disabled by default
  // to ensure the phone grants access.
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, true);
  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, true);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kEnabledByUser, phone_hub_feature);
  }
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 1 /* expected_index */,
      mojom::Feature::kPhoneHub, mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 3 /* expected_index */,
      mojom::Feature::kPhoneHub,
      mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kPhoneHubNotificationsEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 4 /* expected_index */,
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
                     mojom::Feature::kPhoneHubTaskContinuation);
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 6 /* expected_index */,
      mojom::Feature::kPhoneHub, mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kPhoneHubNotificationsAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kPhoneHubNotifications);
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 7 /* expected_index */,
      mojom::Feature::kPhoneHubNotifications,
      mojom::FeatureState::kProhibitedByPolicy);

  // Prohibiting Phone Hub implicitly prohibits all of its sub-features.
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  for (const auto& phone_hub_feature : kAllPhoneHubFeatures) {
    VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                       phone_hub_feature);
  }
  VerifyFeatureStateChange(
      kAllPhoneHubFeatures.size() * 2 + 8 /* expected_index */,
      mojom::Feature::kPhoneHub, mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, CameraRoll) {
  SetupFeatureStateManager();

  TryAllUnverifiedHostStatesAndVerifyFeatureState(
      mojom::Feature::kPhoneHubCameraRoll);

  SetVerifiedHost();
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kPhoneHubCameraRoll);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  SetSoftwareFeatureState(
      true /* use_local_device */,
      multidevice::SoftwareFeature::kPhoneHubCameraRollClient,
      multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kPhoneHubCameraRoll);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kPhoneHubCameraRoll);

  // Camera Roll does not automatically enable with Phone Hub
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, true);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kPhoneHubCameraRoll);

  // Enable Camera Roll
  test_pref_service()->SetBoolean(kPhoneHubCameraRollEnabledPrefName, true);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kPhoneHubCameraRoll);

  // Camera Roll is automatically disabled when Phone Hub is disabled
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kPhoneHubCameraRoll);

  // Camera Roll restores its previous state when Phone Hub is enabled
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, true);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kPhoneHubCameraRoll);

  // Prohibiting Camera Roll does not prohibit Phone Hub
  test_pref_service()->SetBoolean(kPhoneHubCameraRollAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kPhoneHubCameraRoll);

  // Prohibiting Phone Hub does prohibit Camera Roll
  test_pref_service()->SetBoolean(kPhoneHubCameraRollAllowedPrefName, true);
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kPhoneHub);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kPhoneHubCameraRoll);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, WifiSync) {
  SetupFeatureStateManager();
  // Set the initial global state to disabled, so that the wifi sync feature
  // state will be |kDisabledByUser| when it becomes supported on both the host
  // and client devices.
  global_state_feature_managers()[mojom::Feature::kWifiSync]
      ->SetIsFeatureEnabled(false);
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kWifiSync);

  SetVerifiedHost();
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kWifiSync);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kWifiSyncClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kDisabledByUser);

  // The GlobalStateFeatureManager should be updated when the host state changes
  // to |kEnabled|. It will then update the feature state to |kEnabledByUser|.
  global_state_feature_managers()[mojom::Feature::kWifiSync]
      ->SetIsFeatureEnabled(true);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kWifiSyncHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kEnabledByUser);

  // Simulate user toggling the wifi sync state, and verify that the
  // GlobalStateFeatureManager was updated accordingly.
  manager()->SetFeatureEnabledState(mojom::Feature::kWifiSync, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kDisabledByUser);
  EXPECT_FALSE(global_state_feature_managers()[mojom::Feature::kWifiSync]
                   ->IsFeatureEnabled());

  manager()->SetFeatureEnabledState(mojom::Feature::kWifiSync, true);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(5u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kEnabledByUser);
  EXPECT_TRUE(global_state_feature_managers()[mojom::Feature::kWifiSync]
                  ->IsFeatureEnabled());

  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(7u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kWifiSyncAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kWifiSync);
  VerifyFeatureStateChange(8u /* expected_index */, mojom::Feature::kWifiSync,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, Eche) {
  SetupFeatureStateManager();

  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kEche);

  SetVerifiedHost();
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByChromebook);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByPhone);

  // The top-level Phone Hub enabled pref is disabled for existing Better
  // Together users; they must go to settings to explicitly enable PhoneHub.
  test_pref_service()->SetBoolean(kEcheEnabledPrefName, true);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheHost,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(
      5u /* expected_index */, mojom::Feature::kEche,
      mojom::FeatureState::kUnavailableTopLevelFeatureDisabled);

  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, true);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(6u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  VerifyFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(8u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kEcheEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kDisabledByUser,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(9u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kDisabledByUser);

  // Re-enable Eche, then disable Phone Hub, which implicitly
  // makes Eche unavailable.
  test_pref_service()->SetBoolean(kEcheEnabledPrefName, true);
  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(
      11u /* expected_index */, mojom::Feature::kEche,
      mojom::FeatureState::kUnavailableTopLevelFeatureDisabled);

  // Prohibiting Phone Hub implicitly prohibits Eche features.
  test_pref_service()->SetBoolean(kPhoneHubAllowedPrefName, false);
  VerifyFeatureState(mojom::FeatureState::kProhibitedByPolicy,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(12u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest,
       EcheSupportReceivedFromPhoneHub) {
  SetupFeatureStateManager();

  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kEche);

  SetVerifiedHost();
  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kPhoneHubHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByChromebook,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByChromebook);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheClient,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByPhone);

  // The top-level Phone Hub enabled pref is disabled for existing Better
  // Together users; they must go to settings to explicitly enable PhoneHub.
  test_pref_service()->SetBoolean(kEcheEnabledPrefName, true);
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheHost,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kUnavailableTopLevelFeatureDisabled,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(
      5u /* expected_index */, mojom::Feature::kEche,
      mojom::FeatureState::kUnavailableTopLevelFeatureDisabled);

  test_pref_service()->SetBoolean(kPhoneHubEnabledPrefName, true);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(6u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kEnabledByUser);

  // Simulate messages received from PhoneHub which communicate Eche support.
  // No feature state change should occur yet.
  test_pref_service()->SetInteger(
      kEcheOverriddenSupportReceivedFromPhoneHubPrefName,
      static_cast<int>(EcheSupportReceivedFromPhoneHub::kNotSpecified));
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kEche);

  // When the PhoneHub message does not specify support, the value of
  // SoftwareFeature::kEcheHost is consulted, like other features.
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheHost,
                          multidevice::SoftwareFeatureState::kNotSupported);
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(7u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByPhone);

  // Receiving signal of Eche support from PhoneHub should override the value
  // of SoftwareFeature::kEcheHost.
  test_pref_service()->SetInteger(
      kEcheOverriddenSupportReceivedFromPhoneHubPrefName,
      static_cast<int>(EcheSupportReceivedFromPhoneHub::kSupported));
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(8u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kEnabledByUser);

  // Return SoftwareFeature::kEcheHost to "supported" to prepare for next
  // change, but no feature state change should occur yet.
  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kEcheHost,
                          multidevice::SoftwareFeatureState::kSupported);
  VerifyFeatureState(mojom::FeatureState::kEnabledByUser,
                     mojom::Feature::kEche);

  // Receiving signal of Eche support from PhoneHub should override the value
  // of SoftwareFeature::kEcheHost.
  test_pref_service()->SetInteger(
      kEcheOverriddenSupportReceivedFromPhoneHubPrefName,
      static_cast<int>(EcheSupportReceivedFromPhoneHub::kNotSupported));
  VerifyFeatureState(mojom::FeatureState::kNotSupportedByPhone,
                     mojom::Feature::kEche);
  VerifyFeatureStateChange(9u /* expected_index */, mojom::Feature::kEche,
                           mojom::FeatureState::kNotSupportedByPhone);
}

}  // namespace multidevice_setup

}  // namespace ash
