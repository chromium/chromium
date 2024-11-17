// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/multidevice_setup_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_gcm_device_info_provider.h"
#include "chromeos/ash/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/ash/services/multidevice_setup/android_sms_app_installing_status_observer.h"
#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider_impl.h"
#include "chromeos/ash/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/ash/services/multidevice_setup/fake_account_status_change_delegate_notifier.h"
#include "chromeos/ash/services/multidevice_setup/fake_eligible_host_devices_provider.h"
#include "chromeos/ash/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_feature_state_observer.h"
#include "chromeos/ash/services/multidevice_setup/fake_global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_backend_delegate.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_device_timestamp_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_observer.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_verifier.h"
#include "chromeos/ash/services/multidevice_setup/feature_state_manager_impl.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager_impl.h"
#include "chromeos/ash/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"
#include "chromeos/ash/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/ash/services/multidevice_setup/host_device_timestamp_manager_impl.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider_impl.h"
#include "chromeos/ash/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_auth_token_validator.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/ash/services/multidevice_setup/wifi_sync_notification_controller.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 3;

const char kValidAuthToken[] = "validAuthToken";

multidevice::RemoteDeviceList RefListToRawList(
    const multidevice::RemoteDeviceRefList& ref_list) {
  multidevice::RemoteDeviceList raw_list;
  base::ranges::transform(ref_list, std::back_inserter(raw_list),
                          [](const multidevice::RemoteDeviceRef ref) {
                            return *GetMutableRemoteDevice(ref);
                          });
  return raw_list;
}

std::optional<multidevice::RemoteDevice> RefToRaw(
    const std::optional<multidevice::RemoteDeviceRef>& ref) {
  if (!ref)
    return std::nullopt;

  return *GetMutableRemoteDevice(*ref);
}

class FakeEligibleHostDevicesProviderFactory
    : public EligibleHostDevicesProviderImpl::Factory {
 public:
  explicit FakeEligibleHostDevicesProviderFactory(
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : expected_device_sync_client_(expected_device_sync_client) {}

  FakeEligibleHostDevicesProviderFactory(
      const FakeEligibleHostDevicesProviderFactory&) = delete;
  FakeEligibleHostDevicesProviderFactory& operator=(
      const FakeEligibleHostDevicesProviderFactory&) = delete;

  ~FakeEligibleHostDevicesProviderFactory() override = default;

  FakeEligibleHostDevicesProvider* instance() { return instance_; }

 private:
  // EligibleHostDevicesProviderImpl::Factory:
  std::unique_ptr<EligibleHostDevicesProvider> CreateInstance(
      device_sync::DeviceSyncClient* device_sync_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeEligibleHostDevicesProvider>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;

  raw_ptr<FakeEligibleHostDevicesProvider, DanglingUntriaged> instance_ =
      nullptr;
};

class FakeHostBackendDelegateFactory : public HostBackendDelegateImpl::Factory {
 public:
  FakeHostBackendDelegateFactory(
      FakeEligibleHostDevicesProviderFactory*
          fake_eligible_host_devices_provider_factory,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : fake_eligible_host_devices_provider_factory_(
            fake_eligible_host_devices_provider_factory),
        expected_testing_pref_service_(expected_testing_pref_service),
        expected_device_sync_client_(expected_device_sync_client) {}

  FakeHostBackendDelegateFactory(const FakeHostBackendDelegateFactory&) =
      delete;
  FakeHostBackendDelegateFactory& operator=(
      const FakeHostBackendDelegateFactory&) = delete;

  ~FakeHostBackendDelegateFactory() override = default;

  FakeHostBackendDelegate* instance() { return instance_; }

 private:
  // HostBackendDelegateImpl::Factory:
  std::unique_ptr<HostBackendDelegate> CreateInstance(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      std::unique_ptr<base::OneShotTimer> timer) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_eligible_host_devices_provider_factory_->instance(),
              eligible_host_devices_provider);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeHostBackendDelegate>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeEligibleHostDevicesProviderFactory>
      fake_eligible_host_devices_provider_factory_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;
  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;

  raw_ptr<FakeHostBackendDelegate, DanglingUntriaged> instance_ = nullptr;
};

class FakeHostVerifierFactory : public HostVerifierImpl::Factory {
 public:
  FakeHostVerifierFactory(
      FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service)
      : fake_host_backend_delegate_factory_(fake_host_backend_delegate_factory),
        expected_device_sync_client_(expected_device_sync_client),
        expected_testing_pref_service_(expected_testing_pref_service) {}

  FakeHostVerifierFactory(const FakeHostVerifierFactory&) = delete;
  FakeHostVerifierFactory& operator=(const FakeHostVerifierFactory&) = delete;

  ~FakeHostVerifierFactory() override = default;

  FakeHostVerifier* instance() { return instance_; }

 private:
  // HostVerifierImpl::Factory:
  std::unique_ptr<HostVerifier> CreateInstance(
      HostBackendDelegate* host_backend_delegate,
      device_sync::DeviceSyncClient* device_sync_client,
      PrefService* pref_service,
      base::Clock* clock,
      std::unique_ptr<base::OneShotTimer> retry_timer,
      std::unique_ptr<base::OneShotTimer> sync_timer) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_host_backend_delegate_factory_->instance(),
              host_backend_delegate);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);

    auto instance = std::make_unique<FakeHostVerifier>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeHostBackendDelegateFactory> fake_host_backend_delegate_factory_;
  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;

  raw_ptr<FakeHostVerifier, DanglingUntriaged> instance_ = nullptr;
};

class FakeHostStatusProviderFactory : public HostStatusProviderImpl::Factory {
 public:
  FakeHostStatusProviderFactory(
      FakeEligibleHostDevicesProviderFactory*
          fake_eligible_host_devices_provider_factory,
      FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory,
      FakeHostVerifierFactory* fake_host_verifier_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : fake_eligible_host_devices_provider_factory_(
            fake_eligible_host_devices_provider_factory),
        fake_host_backend_delegate_factory_(fake_host_backend_delegate_factory),
        fake_host_verifier_factory_(fake_host_verifier_factory),
        expected_device_sync_client_(expected_device_sync_client) {}

  FakeHostStatusProviderFactory(const FakeHostStatusProviderFactory&) = delete;
  FakeHostStatusProviderFactory& operator=(
      const FakeHostStatusProviderFactory&) = delete;

  ~FakeHostStatusProviderFactory() override = default;

  FakeHostStatusProvider* instance() { return instance_; }

 private:
  // HostStatusProviderImpl::Factory:
  std::unique_ptr<HostStatusProvider> CreateInstance(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      HostBackendDelegate* host_backend_delegate,
      HostVerifier* host_verifier,
      device_sync::DeviceSyncClient* device_sync_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_eligible_host_devices_provider_factory_->instance(),
              eligible_host_devices_provider);
    EXPECT_EQ(fake_host_backend_delegate_factory_->instance(),
              host_backend_delegate);
    EXPECT_EQ(fake_host_verifier_factory_->instance(), host_verifier);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeHostStatusProvider>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeEligibleHostDevicesProviderFactory>
      fake_eligible_host_devices_provider_factory_;
  raw_ptr<FakeHostBackendDelegateFactory> fake_host_backend_delegate_factory_;
  raw_ptr<FakeHostVerifierFactory> fake_host_verifier_factory_;
  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;

  raw_ptr<FakeHostStatusProvider, DanglingUntriaged> instance_ = nullptr;
};

class FakeGlobalStateFeatureManagerFactory
    : public GlobalStateFeatureManagerImpl::Factory {
 public:
  FakeGlobalStateFeatureManagerFactory(
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : fake_host_status_provider_factory_(fake_host_status_provider_factory),
        expected_testing_pref_service_(expected_testing_pref_service),
        expected_device_sync_client_(expected_device_sync_client) {}

  FakeGlobalStateFeatureManagerFactory(
      const FakeGlobalStateFeatureManagerFactory&) = delete;
  FakeGlobalStateFeatureManagerFactory& operator=(
      const FakeGlobalStateFeatureManagerFactory&) = delete;
  ~FakeGlobalStateFeatureManagerFactory() override = default;

 private:
  // GlobalStateFeatureManagerImpl::Factory:
  std::unique_ptr<GlobalStateFeatureManager> CreateInstance(
      GlobalStateFeatureManagerImpl::Factory::Option option,
      HostStatusProvider* host_status_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      std::unique_ptr<base::OneShotTimer> timer) override {
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    return std::make_unique<FakeGlobalStateFeatureManager>();
  }

  raw_ptr<FakeHostStatusProviderFactory> fake_host_status_provider_factory_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;
  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;
};

class FakeWifiSyncNotificationControllerFactory
    : public WifiSyncNotificationController::Factory {
 public:
  FakeWifiSyncNotificationControllerFactory(
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : fake_host_status_provider_factory_(fake_host_status_provider_factory),
        expected_testing_pref_service_(expected_testing_pref_service),
        expected_device_sync_client_(expected_device_sync_client) {}

  FakeWifiSyncNotificationControllerFactory(
      const FakeWifiSyncNotificationControllerFactory&) = delete;
  FakeWifiSyncNotificationControllerFactory& operator=(
      const FakeWifiSyncNotificationControllerFactory&) = delete;
  ~FakeWifiSyncNotificationControllerFactory() override = default;

 private:
  // WifiSyncNotificationController::Factory:
  std::unique_ptr<WifiSyncNotificationController> CreateInstance(
      GlobalStateFeatureManager* wifi_sync_feature_manager,
      HostStatusProvider* host_status_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      AccountStatusChangeDelegateNotifier* delegate_notifier) override {
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    // Only check inputs and return nullptr. We do not want to trigger any logic
    // in these unit tests.
    return nullptr;
  }

  raw_ptr<FakeHostStatusProviderFactory> fake_host_status_provider_factory_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;
  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;
};

class FakeGrandfatheredEasyUnlockHostDisablerFactory
    : public GrandfatheredEasyUnlockHostDisabler::Factory {
 public:
  FakeGrandfatheredEasyUnlockHostDisablerFactory(
      FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service)
      : fake_host_backend_delegate_factory_(fake_host_backend_delegate_factory),
        expected_device_sync_client_(expected_device_sync_client),
        expected_testing_pref_service_(expected_testing_pref_service) {}

  FakeGrandfatheredEasyUnlockHostDisablerFactory(
      const FakeGrandfatheredEasyUnlockHostDisablerFactory&) = delete;
  FakeGrandfatheredEasyUnlockHostDisablerFactory& operator=(
      const FakeGrandfatheredEasyUnlockHostDisablerFactory&) = delete;

  ~FakeGrandfatheredEasyUnlockHostDisablerFactory() override = default;

 private:
  // GrandfatheredEasyUnlockHostDisabler::Factory:
  std::unique_ptr<GrandfatheredEasyUnlockHostDisabler> CreateInstance(
      HostBackendDelegate* host_backend_delegate,
      device_sync::DeviceSyncClient* device_sync_client,
      PrefService* pref_service,
      std::unique_ptr<base::OneShotTimer> timer) override {
    EXPECT_EQ(fake_host_backend_delegate_factory_->instance(),
              host_backend_delegate);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    // Only check inputs and return nullptr. We do not want to trigger any logic
    // in these unit tests.
    return nullptr;
  }

  raw_ptr<FakeHostBackendDelegateFactory> fake_host_backend_delegate_factory_;
  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;
};

class FakeFeatureStateManagerFactory : public FeatureStateManagerImpl::Factory {
 public:
  FakeFeatureStateManagerFactory(
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      FakeAndroidSmsPairingStateTracker*
          expected_android_sms_pairing_state_tracker,
      bool expected_is_secondary_user)
      : expected_testing_pref_service_(expected_testing_pref_service),
        fake_host_status_provider_factory_(fake_host_status_provider_factory),
        expected_device_sync_client_(expected_device_sync_client),
        expected_android_sms_pairing_state_tracker_(
            expected_android_sms_pairing_state_tracker),
        expected_is_secondary_user_(expected_is_secondary_user) {}

  FakeFeatureStateManagerFactory(const FakeFeatureStateManagerFactory&) =
      delete;
  FakeFeatureStateManagerFactory& operator=(
      const FakeFeatureStateManagerFactory&) = delete;

  ~FakeFeatureStateManagerFactory() override = default;

  FakeFeatureStateManager* instance() { return instance_; }

 private:
  // FeatureStateManagerImpl::Factory:
  std::unique_ptr<FeatureStateManager> CreateInstance(
      PrefService* pref_service,
      HostStatusProvider* host_status_provider,
      device_sync::DeviceSyncClient* device_sync_client,
      AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
      const base::flat_map<mojom::Feature,
                           raw_ptr<GlobalStateFeatureManager, CtnExperimental>>&
          global_state_feature_managers,
      bool is_secondary_user) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_android_sms_pairing_state_tracker_,
              android_sms_pairing_state_tracker);
    EXPECT_EQ(expected_is_secondary_user_, is_secondary_user);

    auto instance = std::make_unique<FakeFeatureStateManager>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;
  raw_ptr<FakeHostStatusProviderFactory> fake_host_status_provider_factory_;
  raw_ptr<device_sync::FakeDeviceSyncClient> expected_device_sync_client_;
  raw_ptr<FakeAndroidSmsPairingStateTracker, DanglingUntriaged>
      expected_android_sms_pairing_state_tracker_;
  bool expected_is_secondary_user_;

  raw_ptr<FakeFeatureStateManager, DanglingUntriaged> instance_ = nullptr;
};

class FakeHostDeviceTimestampManagerFactory
    : public HostDeviceTimestampManagerImpl::Factory {
 public:
  FakeHostDeviceTimestampManagerFactory(
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service)
      : fake_host_status_provider_factory_(fake_host_status_provider_factory),
        expected_testing_pref_service_(expected_testing_pref_service) {}

  FakeHostDeviceTimestampManagerFactory(
      const FakeHostDeviceTimestampManagerFactory&) = delete;
  FakeHostDeviceTimestampManagerFactory& operator=(
      const FakeHostDeviceTimestampManagerFactory&) = delete;

  ~FakeHostDeviceTimestampManagerFactory() override = default;

  FakeHostDeviceTimestampManager* instance() { return instance_; }

 private:
  // HostDeviceTimestampManagerImpl::Factory:
  std::unique_ptr<HostDeviceTimestampManager> CreateInstance(
      HostStatusProvider* host_status_provider,
      PrefService* pref_service,
      base::Clock* clock) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);

    auto instance = std::make_unique<FakeHostDeviceTimestampManager>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeHostStatusProviderFactory> fake_host_status_provider_factory_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;

  raw_ptr<FakeHostDeviceTimestampManager, DanglingUntriaged> instance_ =
      nullptr;
};

class FakeAccountStatusChangeDelegateNotifierFactory
    : public AccountStatusChangeDelegateNotifierImpl::Factory {
 public:
  FakeAccountStatusChangeDelegateNotifierFactory(
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      FakeHostDeviceTimestampManagerFactory*
          fake_host_device_timestamp_manager_factory,
      OobeCompletionTracker* expected_oobe_completion_tracker)
      : fake_host_status_provider_factory_(fake_host_status_provider_factory),
        expected_testing_pref_service_(expected_testing_pref_service),
        fake_host_device_timestamp_manager_factory_(
            fake_host_device_timestamp_manager_factory),
        expected_oobe_completion_tracker_(expected_oobe_completion_tracker) {}

  FakeAccountStatusChangeDelegateNotifierFactory(
      const FakeAccountStatusChangeDelegateNotifierFactory&) = delete;
  FakeAccountStatusChangeDelegateNotifierFactory& operator=(
      const FakeAccountStatusChangeDelegateNotifierFactory&) = delete;

  ~FakeAccountStatusChangeDelegateNotifierFactory() override = default;

  FakeAccountStatusChangeDelegateNotifier* instance() { return instance_; }

 private:
  // AccountStatusChangeDelegateNotifierImpl::Factory:
  std::unique_ptr<AccountStatusChangeDelegateNotifier> CreateInstance(
      HostStatusProvider* host_status_provider,
      PrefService* pref_service,
      HostDeviceTimestampManager* host_device_timestamp_manager,
      OobeCompletionTracker* oobe_completion_tracker,
      base::Clock* clock) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(fake_host_device_timestamp_manager_factory_->instance(),
              host_device_timestamp_manager);
    EXPECT_EQ(expected_oobe_completion_tracker_, oobe_completion_tracker);

    auto instance = std::make_unique<FakeAccountStatusChangeDelegateNotifier>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeHostStatusProviderFactory> fake_host_status_provider_factory_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable>
      expected_testing_pref_service_;
  raw_ptr<FakeHostDeviceTimestampManagerFactory>
      fake_host_device_timestamp_manager_factory_;
  raw_ptr<OobeCompletionTracker> expected_oobe_completion_tracker_;

  raw_ptr<FakeAccountStatusChangeDelegateNotifier, DanglingUntriaged>
      instance_ = nullptr;
};

class FakeAndroidSmsAppInstallingStatusObserverFactory
    : public AndroidSmsAppInstallingStatusObserver::Factory {
 public:
  FakeAndroidSmsAppInstallingStatusObserverFactory(
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      FakeFeatureStateManagerFactory* fake_feature_state_manager_factory,
      AndroidSmsAppHelperDelegate* expected_android_sms_app_helper_delegate)
      : fake_host_status_provider_factory_(fake_host_status_provider_factory),
        fake_feature_state_manager_factory_(fake_feature_state_manager_factory),
        expected_android_sms_app_helper_delegate_(
            expected_android_sms_app_helper_delegate) {}

  FakeAndroidSmsAppInstallingStatusObserverFactory(
      const FakeAndroidSmsAppInstallingStatusObserverFactory&) = delete;
  FakeAndroidSmsAppInstallingStatusObserverFactory& operator=(
      const FakeAndroidSmsAppInstallingStatusObserverFactory&) = delete;

  ~FakeAndroidSmsAppInstallingStatusObserverFactory() override = default;

 private:
  // AndroidSmsAppInstallingStatusObserver::Factory:
  std::unique_ptr<AndroidSmsAppInstallingStatusObserver> CreateInstance(
      HostStatusProvider* host_status_provider,
      FeatureStateManager* feature_state_manager,
      AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate) override {
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(fake_feature_state_manager_factory_->instance(),
              feature_state_manager);
    EXPECT_EQ(expected_android_sms_app_helper_delegate_,
              android_sms_app_helper_delegate);
    // Only check inputs and return nullptr. We do not want to trigger the
    // AndroidSmsAppInstallingStatusObserver logic in these unit tests.
    return nullptr;
  }

  raw_ptr<FakeHostStatusProviderFactory> fake_host_status_provider_factory_;
  raw_ptr<FakeFeatureStateManagerFactory> fake_feature_state_manager_factory_;
  raw_ptr<AndroidSmsAppHelperDelegate, DanglingUntriaged>
      expected_android_sms_app_helper_delegate_;
};

}  // namespace

class MultiDeviceSetupImplTest : public ::testing::TestWithParam<bool> {
 public:
  MultiDeviceSetupImplTest(const MultiDeviceSetupImplTest&) = delete;
  MultiDeviceSetupImplTest& operator=(const MultiDeviceSetupImplTest&) = delete;

 protected:
  MultiDeviceSetupImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupImplTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();

    fake_auth_token_validator_ = std::make_unique<FakeAuthTokenValidator>();
    fake_auth_token_validator_->set_expected_auth_token(kValidAuthToken);

    fake_oobe_completion_tracker_ = std::make_unique<OobeCompletionTracker>();

    fake_android_sms_app_helper_delegate_ =
        std::make_unique<FakeAndroidSmsAppHelperDelegate>();

    fake_android_sms_pairing_state_tracker_ =
        std::make_unique<FakeAndroidSmsPairingStateTracker>();

    fake_gcm_device_info_provider_ =
        std::make_unique<device_sync::FakeGcmDeviceInfoProvider>(
            cryptauth::GcmDeviceInfo());

    fake_eligible_host_devices_provider_factory_ =
        std::make_unique<FakeEligibleHostDevicesProviderFactory>(
            fake_device_sync_client_.get());
    EligibleHostDevicesProviderImpl::Factory::SetFactoryForTesting(
        fake_eligible_host_devices_provider_factory_.get());

    fake_host_backend_delegate_factory_ =
        std::make_unique<FakeHostBackendDelegateFactory>(
            fake_eligible_host_devices_provider_factory_.get(),
            test_pref_service_.get(), fake_device_sync_client_.get());
    HostBackendDelegateImpl::Factory::SetFactoryForTesting(
        fake_host_backend_delegate_factory_.get());

    fake_host_verifier_factory_ = std::make_unique<FakeHostVerifierFactory>(
        fake_host_backend_delegate_factory_.get(),
        fake_device_sync_client_.get(), test_pref_service_.get());
    HostVerifierImpl::Factory::SetFactoryForTesting(
        fake_host_verifier_factory_.get());

    fake_host_status_provider_factory_ =
        std::make_unique<FakeHostStatusProviderFactory>(
            fake_eligible_host_devices_provider_factory_.get(),
            fake_host_backend_delegate_factory_.get(),
            fake_host_verifier_factory_.get(), fake_device_sync_client_.get());
    HostStatusProviderImpl::Factory::SetFactoryForTesting(
        fake_host_status_provider_factory_.get());

    fake_global_state_feature_manager_factory_ =
        std::make_unique<FakeGlobalStateFeatureManagerFactory>(
            fake_host_status_provider_factory_.get(), test_pref_service_.get(),
            fake_device_sync_client_.get());
    GlobalStateFeatureManagerImpl::Factory::SetFactoryForTesting(
        fake_global_state_feature_manager_factory_.get());

    fake_wifi_sync_notification_controller_factory_ =
        std::make_unique<FakeWifiSyncNotificationControllerFactory>(
            fake_host_status_provider_factory_.get(), test_pref_service_.get(),
            fake_device_sync_client_.get());
    WifiSyncNotificationController::Factory::SetFactoryForTesting(
        fake_wifi_sync_notification_controller_factory_.get());

    fake_grandfathered_easy_unlock_host_disabler_factory_ =
        std::make_unique<FakeGrandfatheredEasyUnlockHostDisablerFactory>(
            fake_host_backend_delegate_factory_.get(),
            fake_device_sync_client_.get(), test_pref_service_.get());
    GrandfatheredEasyUnlockHostDisabler::Factory::SetFactoryForTesting(
        fake_grandfathered_easy_unlock_host_disabler_factory_.get());

    fake_feature_state_manager_factory_ =
        std::make_unique<FakeFeatureStateManagerFactory>(
            test_pref_service_.get(), fake_host_status_provider_factory_.get(),
            fake_device_sync_client_.get(),
            fake_android_sms_pairing_state_tracker_.get(), is_secondary_user_);
    FeatureStateManagerImpl::Factory::SetFactoryForTesting(
        fake_feature_state_manager_factory_.get());

    fake_host_device_timestamp_manager_factory_ =
        std::make_unique<FakeHostDeviceTimestampManagerFactory>(
            fake_host_status_provider_factory_.get(), test_pref_service_.get());
    HostDeviceTimestampManagerImpl::Factory::SetFactoryForTesting(
        fake_host_device_timestamp_manager_factory_.get());

    fake_account_status_change_delegate_notifier_factory_ =
        std::make_unique<FakeAccountStatusChangeDelegateNotifierFactory>(
            fake_host_status_provider_factory_.get(), test_pref_service_.get(),
            fake_host_device_timestamp_manager_factory_.get(),
            fake_oobe_completion_tracker_.get());
    AccountStatusChangeDelegateNotifierImpl::Factory::SetFactoryForTesting(
        fake_account_status_change_delegate_notifier_factory_.get());

    fake_android_sms_app_installing_status_observer_factory_ =
        std::make_unique<FakeAndroidSmsAppInstallingStatusObserverFactory>(
            fake_host_status_provider_factory_.get(),
            fake_feature_state_manager_factory_.get(),
            fake_android_sms_app_helper_delegate_.get());
    AndroidSmsAppInstallingStatusObserver::Factory::SetFactoryForTesting(
        fake_android_sms_app_installing_status_observer_factory_.get());

    multidevice_setup_ = MultiDeviceSetupImpl::Factory::Create(
        test_pref_service_.get(), fake_device_sync_client_.get(),
        fake_auth_token_validator_.get(), fake_oobe_completion_tracker_.get(),
        fake_android_sms_app_helper_delegate_.get(),
        fake_android_sms_pairing_state_tracker_.get(),
        fake_gcm_device_info_provider_.get(), is_secondary_user_);
  }

  void TearDown() override {
    EligibleHostDevicesProviderImpl::Factory::SetFactoryForTesting(nullptr);
    HostBackendDelegateImpl::Factory::SetFactoryForTesting(nullptr);
    HostVerifierImpl::Factory::SetFactoryForTesting(nullptr);
    HostStatusProviderImpl::Factory::SetFactoryForTesting(nullptr);
    GrandfatheredEasyUnlockHostDisabler::Factory::SetFactoryForTesting(nullptr);
    FeatureStateManagerImpl::Factory::SetFactoryForTesting(nullptr);
    HostDeviceTimestampManagerImpl::Factory::SetFactoryForTesting(nullptr);
    AccountStatusChangeDelegateNotifierImpl::Factory::SetFactoryForTesting(
        nullptr);
    AndroidSmsAppInstallingStatusObserver::Factory::SetFactoryForTesting(
        nullptr);
    GlobalStateFeatureManagerImpl::Factory::SetFactoryForTesting(nullptr);
    WifiSyncNotificationController::Factory::SetFactoryForTesting(nullptr);
  }

  void CallSetAccountStatusChangeDelegate() {
    EXPECT_FALSE(fake_account_status_change_delegate_);

    fake_account_status_change_delegate_ =
        std::make_unique<FakeAccountStatusChangeDelegate>();

    EXPECT_FALSE(fake_account_status_change_delegate_notifier()->delegate());
    multidevice_setup_->SetAccountStatusChangeDelegate(
        fake_account_status_change_delegate_->GenerateRemote());
    EXPECT_TRUE(fake_account_status_change_delegate_notifier()->delegate());
  }

  multidevice::RemoteDeviceList CallGetEligibleHostDevices() {
    base::RunLoop run_loop;
    multidevice_setup_->GetEligibleHostDevices(
        base::BindOnce(&MultiDeviceSetupImplTest::OnEligibleDevicesFetched,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    multidevice::RemoteDeviceList eligible_devices_list =
        *last_eligible_devices_list_;
    last_eligible_devices_list_.reset();

    return eligible_devices_list;
  }

  std::vector<mojom::HostDevicePtr> CallGetEligibleActiveHostDevices() {
    base::RunLoop run_loop;
    multidevice_setup_->GetEligibleActiveHostDevices(base::BindOnce(
        &MultiDeviceSetupImplTest::OnEligibleActiveHostDevicesFetched,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    std::vector<mojom::HostDevicePtr> eligible_devices_list =
        std::move(*last_eligible_active_devices_list_);
    last_eligible_active_devices_list_.reset();

    return eligible_devices_list;
  }

  bool CallSetHostDevice(
      const std::string& host_instance_id_or_legacy_device_id,
      const std::string& auth_token) {
    base::RunLoop run_loop;
    multidevice_setup_->SetHostDevice(
        host_instance_id_or_legacy_device_id, auth_token,
        base::BindOnce(&MultiDeviceSetupImplTest::OnSetHostDeviceResult,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_set_host_success_;
    last_set_host_success_.reset();

    return success;
  }

  bool CallSetHostDeviceWithoutAuth(
      const std::string& host_instance_id_or_legacy_device_id) {
    base::RunLoop run_loop;
    multidevice_setup_->SetHostDeviceWithoutAuthToken(
        host_instance_id_or_legacy_device_id,
        base::BindOnce(
            &MultiDeviceSetupImplTest::OnSetHostDeviceWithoutAuthResult,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_set_host_without_auth_success_;
    last_set_host_without_auth_success_.reset();

    return success;
  }

  std::pair<mojom::HostStatus, std::optional<multidevice::RemoteDevice>>
  CallGetHostStatus() {
    base::RunLoop run_loop;
    multidevice_setup_->GetHostStatus(
        base::BindOnce(&MultiDeviceSetupImplTest::OnHostStatusReceived,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    std::pair<mojom::HostStatus, std::optional<multidevice::RemoteDevice>>
        host_status_update = *last_host_status_;
    last_host_status_.reset();

    return host_status_update;
  }

  bool CallSetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      const std::optional<std::string>& auth_token) {
    base::RunLoop run_loop;
    multidevice_setup_->SetFeatureEnabledState(
        feature, enabled, auth_token,
        base::BindOnce(&MultiDeviceSetupImplTest::OnSetFeatureEnabled,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_set_feature_enabled_state_success_;
    last_set_feature_enabled_state_success_.reset();

    return success;
  }

  base::flat_map<mojom::Feature, mojom::FeatureState> CallGetFeatureStates() {
    base::RunLoop run_loop;
    multidevice_setup_->GetFeatureStates(
        base::BindOnce(&MultiDeviceSetupImplTest::OnGetFeatureStates,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    base::flat_map<mojom::Feature, mojom::FeatureState> feature_states_map =
        *last_get_feature_states_result_;
    last_get_feature_states_result_.reset();

    return feature_states_map;
  }

  bool CallRetrySetHostNow() {
    base::RunLoop run_loop;
    multidevice_setup_->RetrySetHostNow(
        base::BindOnce(&MultiDeviceSetupImplTest::OnHostRetried,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_retry_success_;
    last_retry_success_.reset();

    return success;
  }

  bool CallTriggerEventForDebugging(mojom::EventTypeForDebugging type) {
    base::RunLoop run_loop;
    multidevice_setup_->TriggerEventForDebugging(
        type, base::BindOnce(&MultiDeviceSetupImplTest::OnDebugEventTriggered,
                             base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_debug_event_success_;
    last_debug_event_success_.reset();

    // If the delegate was set, fire off any pending Mojo messages.
    if (success)
      fake_account_status_change_delegate_notifier()->FlushForTesting();

    return success;
  }

  std::optional<std::string> CallGetQuickStartPhoneInstanceID() {
    base::RunLoop run_loop;
    multidevice_setup_->GetQuickStartPhoneInstanceID(base::BindOnce(
        &MultiDeviceSetupImplTest::OnGetQuickStartPhoneInstanceID,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    std::optional<std::string> qs_phone_instance_id =
        last_qs_phone_instance_id_;
    last_qs_phone_instance_id_.reset();
    return qs_phone_instance_id;
  }

  void VerifyCurrentHostStatus(
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device,
      FakeHostStatusObserver* observer = nullptr,
      size_t expected_observer_index = 0u) {
    std::pair<mojom::HostStatus, std::optional<multidevice::RemoteDevice>>
        host_status_and_device = CallGetHostStatus();
    EXPECT_EQ(host_status, host_status_and_device.first);
    EXPECT_EQ(RefToRaw(host_device), host_status_and_device.second);

    if (!observer)
      return;

    EXPECT_EQ(host_status,
              observer->host_status_updates()[expected_observer_index].first);
    EXPECT_EQ(RefToRaw(host_device),
              observer->host_status_updates()[expected_observer_index].second);
  }

  void SendPendingObserverMessages() {
    MultiDeviceSetupImpl* derived_ptr =
        static_cast<MultiDeviceSetupImpl*>(multidevice_setup_.get());
    derived_ptr->FlushForTesting();
  }

  FakeAccountStatusChangeDelegate* fake_account_status_change_delegate() {
    return fake_account_status_change_delegate_.get();
  }

  FakeEligibleHostDevicesProvider* fake_eligible_host_devices_provider() {
    return fake_eligible_host_devices_provider_factory_->instance();
  }

  FakeHostBackendDelegate* fake_host_backend_delegate() {
    return fake_host_backend_delegate_factory_->instance();
  }

  FakeHostVerifier* fake_host_verifier() {
    return fake_host_verifier_factory_->instance();
  }

  FakeHostStatusProvider* fake_host_status_provider() {
    return fake_host_status_provider_factory_->instance();
  }

  FakeFeatureStateManager* fake_feature_state_manager() {
    return fake_feature_state_manager_factory_->instance();
  }

  FakeHostDeviceTimestampManager* fake_host_device_timestamp_manager() {
    return fake_host_device_timestamp_manager_factory_->instance();
  }

  FakeAccountStatusChangeDelegateNotifier*
  fake_account_status_change_delegate_notifier() {
    return fake_account_status_change_delegate_notifier_factory_->instance();
  }

  multidevice::RemoteDeviceRefList& test_devices() { return test_devices_; }

  MultiDeviceSetupBase* multidevice_setup() { return multidevice_setup_.get(); }

 private:
  void OnEligibleDevicesFetched(
      base::OnceClosure quit_closure,
      const multidevice::RemoteDeviceList& eligible_devices_list) {
    EXPECT_FALSE(last_eligible_devices_list_);
    last_eligible_devices_list_ = eligible_devices_list;
    std::move(quit_closure).Run();
  }

  void OnEligibleActiveHostDevicesFetched(
      base::OnceClosure quit_closure,
      std::vector<mojom::HostDevicePtr> eligible_active_devices_list) {
    EXPECT_FALSE(last_eligible_active_devices_list_);
    last_eligible_active_devices_list_ =
        std::move(eligible_active_devices_list);
    std::move(quit_closure).Run();
  }

  void OnSetHostDeviceResult(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_set_host_success_);
    last_set_host_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnSetHostDeviceWithoutAuthResult(base::OnceClosure quit_closure,
                                        bool success) {
    EXPECT_FALSE(last_set_host_without_auth_success_);
    last_set_host_without_auth_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnHostStatusReceived(
      base::OnceClosure quit_closure,
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDevice>& host_device) {
    EXPECT_FALSE(last_host_status_);
    last_host_status_ = std::make_pair(host_status, host_device);
    std::move(quit_closure).Run();
  }

  void OnSetFeatureEnabled(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_set_feature_enabled_state_success_);
    last_set_feature_enabled_state_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnGetFeatureStates(
      base::OnceClosure quit_closure,
      const base::flat_map<mojom::Feature, mojom::FeatureState>&
          feature_states_map) {
    EXPECT_FALSE(last_get_feature_states_result_);
    last_get_feature_states_result_ = feature_states_map;
    std::move(quit_closure).Run();
  }

  void OnHostRetried(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_retry_success_);
    last_retry_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnDebugEventTriggered(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_debug_event_success_);
    last_debug_event_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnGetQuickStartPhoneInstanceID(
      base::OnceClosure quit_closure,
      const std::optional<std::string>& qs_phone_instance_id) {
    EXPECT_EQ(std::nullopt, last_qs_phone_instance_id_);
    last_qs_phone_instance_id_ = qs_phone_instance_id;
    std::move(quit_closure).Run();
  }

  base::test::TaskEnvironment task_environment_;

  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeAuthTokenValidator> fake_auth_token_validator_;
  std::unique_ptr<OobeCompletionTracker> fake_oobe_completion_tracker_;
  std::unique_ptr<device_sync::FakeGcmDeviceInfoProvider>
      fake_gcm_device_info_provider_;
  bool is_secondary_user_ = false;

  std::unique_ptr<FakeEligibleHostDevicesProviderFactory>
      fake_eligible_host_devices_provider_factory_;
  std::unique_ptr<FakeHostBackendDelegateFactory>
      fake_host_backend_delegate_factory_;
  std::unique_ptr<FakeHostVerifierFactory> fake_host_verifier_factory_;
  std::unique_ptr<FakeHostStatusProviderFactory>
      fake_host_status_provider_factory_;
  std::unique_ptr<FakeGlobalStateFeatureManagerFactory>
      fake_global_state_feature_manager_factory_;
  std::unique_ptr<FakeWifiSyncNotificationControllerFactory>
      fake_wifi_sync_notification_controller_factory_;
  std::unique_ptr<FakeGrandfatheredEasyUnlockHostDisablerFactory>
      fake_grandfathered_easy_unlock_host_disabler_factory_;
  std::unique_ptr<FakeFeatureStateManagerFactory>
      fake_feature_state_manager_factory_;
  std::unique_ptr<FakeHostDeviceTimestampManagerFactory>
      fake_host_device_timestamp_manager_factory_;
  std::unique_ptr<FakeAccountStatusChangeDelegateNotifierFactory>
      fake_account_status_change_delegate_notifier_factory_;
  std::unique_ptr<FakeAndroidSmsAppInstallingStatusObserverFactory>
      fake_android_sms_app_installing_status_observer_factory_;
  std::unique_ptr<FakeAndroidSmsAppHelperDelegate>
      fake_android_sms_app_helper_delegate_;
  std::unique_ptr<FakeAndroidSmsPairingStateTracker>
      fake_android_sms_pairing_state_tracker_;

  std::unique_ptr<FakeAccountStatusChangeDelegate>
      fake_account_status_change_delegate_;

  std::optional<bool> last_debug_event_success_;
  std::optional<multidevice::RemoteDeviceList> last_eligible_devices_list_;
  std::optional<std::vector<mojom::HostDevicePtr>>
      last_eligible_active_devices_list_;
  std::optional<bool> last_set_host_success_;
  std::optional<bool> last_set_host_without_auth_success_;
  std::optional<
      std::pair<mojom::HostStatus, std::optional<multidevice::RemoteDevice>>>
      last_host_status_;
  std::optional<bool> last_set_feature_enabled_state_success_;
  std::optional<base::flat_map<mojom::Feature, mojom::FeatureState>>
      last_get_feature_states_result_;
  std::optional<bool> last_retry_success_;
  std::optional<std::string> last_qs_phone_instance_id_;

  std::unique_ptr<MultiDeviceSetupBase> multidevice_setup_;
};

TEST_F(MultiDeviceSetupImplTest, AccountStatusChangeDelegate) {
  // All requests to trigger debug events should fail before the delegate has
  // been set.
  EXPECT_FALSE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists));
  EXPECT_FALSE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched));
  EXPECT_FALSE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded));

  CallSetAccountStatusChangeDelegate();

  // All debug trigger events should now succeed.
  EXPECT_TRUE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists));
  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_new_user_potential_host_events_handled());

  EXPECT_TRUE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched));
  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_existing_user_host_switched_events_handled());

  EXPECT_TRUE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded));
  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_existing_user_chromebook_added_events_handled());
}

// The feature mojom::Feature::kInstantTethering is used throughout this test
// because it never requires authentication for either enabling or disabling.
TEST_F(MultiDeviceSetupImplTest, FeatureStateChanges_NoAuthTokenRequired) {
  auto observer = std::make_unique<FakeFeatureStateObserver>();
  multidevice_setup()->AddFeatureStateObserver(observer->GenerateRemote());

  EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
            CallGetFeatureStates()[mojom::Feature::kInstantTethering]);

  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kInstantTethering,
      mojom::FeatureState::kNotSupportedByChromebook);
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            CallGetFeatureStates()[mojom::Feature::kInstantTethering]);
  EXPECT_EQ(1u, observer->feature_state_updates().size());

  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kInstantTethering, mojom::FeatureState::kEnabledByUser);
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            CallGetFeatureStates()[mojom::Feature::kInstantTethering]);
  EXPECT_EQ(2u, observer->feature_state_updates().size());

  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kInstantTethering,
                                         false /* enabled */,
                                         std::nullopt /* auth_token */));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kInstantTethering]);
  EXPECT_EQ(3u, observer->feature_state_updates().size());
}

// mojom::Feature::kSmartLock requires authentication when attempting to enable.
TEST_F(MultiDeviceSetupImplTest,
       FeatureStateChanges_AuthTokenRequired_SmartLock) {
  auto observer = std::make_unique<FakeFeatureStateObserver>();
  multidevice_setup()->AddFeatureStateObserver(observer->GenerateRemote());

  EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
            CallGetFeatureStates()[mojom::Feature::kSmartLock]);

  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kSmartLock, mojom::FeatureState::kEnabledByUser);
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            CallGetFeatureStates()[mojom::Feature::kSmartLock]);
  EXPECT_EQ(1u, observer->feature_state_updates().size());

  // No authentication is required to disable the feature.
  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kSmartLock,
                                         false /* enabled */,
                                         std::nullopt /* auth_token */));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kSmartLock]);
  EXPECT_EQ(2u, observer->feature_state_updates().size());

  // However, authentication is required to enable the feature.
  EXPECT_FALSE(CallSetFeatureEnabledState(
      mojom::Feature::kSmartLock, true /* enabled */, "invalidAuthToken"));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kSmartLock]);
  EXPECT_EQ(2u, observer->feature_state_updates().size());

  // Now, send a valid auth token; it should successfully enable.
  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kSmartLock,
                                         true /* enabled */, kValidAuthToken));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            CallGetFeatureStates()[mojom::Feature::kSmartLock]);
  EXPECT_EQ(3u, observer->feature_state_updates().size());
}

// mojom::Feature::kBetterTogetherSuite requires authentication when attempting
// to enable, but only if the Smart Lock pref is enabled.
TEST_F(MultiDeviceSetupImplTest,
       FeatureStateChanges_AuthTokenRequired_BetterTogetherSuite) {
  auto observer = std::make_unique<FakeFeatureStateObserver>();
  multidevice_setup()->AddFeatureStateObserver(observer->GenerateRemote());

  EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);

  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kBetterTogetherSuite,
      mojom::FeatureState::kEnabledByUser);
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(1u, observer->feature_state_updates().size());

  // No authentication is required to disable the feature.
  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                                         false /* enabled */,
                                         std::nullopt /* auth_token */));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(2u, observer->feature_state_updates().size());

  // Authentication is required to enable the feature if SmartLock's state is
  // kUnavailableInsufficientSecurity.
  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kSmartLock,
      mojom::FeatureState::kUnavailableInsufficientSecurity);
  EXPECT_FALSE(CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                                          true /* enabled */,
                                          "invalidAuthToken"));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(3u, observer->feature_state_updates().size());

  // Now, send a valid auth token; it should successfully enable.
  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                                         true /* enabled */, kValidAuthToken));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(4u, observer->feature_state_updates().size());

  // Disable one more time.
  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                                         false /* enabled */,
                                         std::nullopt /* auth_token */));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(5u, observer->feature_state_updates().size());

  // Authentication is required to enable the feature if SmartLock's state is
  // kUnavailableSuiteDisabled.
  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kSmartLock,
      mojom::FeatureState::kUnavailableSuiteDisabled);
  EXPECT_FALSE(CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                                          true /* enabled */,
                                          "invalidAuthToken"));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(6u, observer->feature_state_updates().size());

  // Now, send a valid auth token; it should successfully enable.
  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                                         true /* enabled */, kValidAuthToken));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(7u, observer->feature_state_updates().size());
}

TEST_F(MultiDeviceSetupImplTest, ComprehensiveHostTest) {
  // Start with no eligible devices.
  EXPECT_TRUE(CallGetEligibleHostDevices().empty());
  VerifyCurrentHostStatus(mojom::HostStatus::kNoEligibleHosts,
                          std::nullopt /* host_device */);

  // Cannot retry without a host.
  EXPECT_FALSE(CallRetrySetHostNow());

  // Add a status observer.
  auto observer = std::make_unique<FakeHostStatusObserver>();
  multidevice_setup()->AddHostStatusObserver(observer->GenerateRemote());

  // Simulate a sync occurring; now, all of the test devices are eligible hosts.
  fake_eligible_host_devices_provider()->set_eligible_host_devices(
      test_devices());
  EXPECT_EQ(RefListToRawList(test_devices()), CallGetEligibleHostDevices());
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      std::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          std::nullopt /* host_device */, observer.get(),
                          0u /* expected_observer_index */);

  // There are eligible hosts, but none is set; thus, cannot retry.
  EXPECT_FALSE(CallRetrySetHostNow());

  // Set an invalid host as the host device; this should fail.
  EXPECT_FALSE(CallSetHostDevice("invalidHostDeviceId", kValidAuthToken));
  EXPECT_FALSE(fake_host_backend_delegate()->HasPendingHostRequest());

  // Set device 0 as the host; this should succeed.
  std::string host_id = test_devices()[0].instance_id();
  EXPECT_TRUE(CallSetHostDevice(host_id, kValidAuthToken));
  EXPECT_TRUE(fake_host_backend_delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0],
            fake_host_backend_delegate()->GetPendingHostRequest());
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0], observer.get(), 1u /* expected_observer_index */);

  // It should now be possible to retry.
  EXPECT_TRUE(CallRetrySetHostNow());

  // Simulate the retry succeeding and the host being set on the back-end.
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kHostSetButNotYetVerified,
                          test_devices()[0], observer.get(),
                          2u /* expected_observer_index */);

  // It should still be possible to retry (this time, retrying verification).
  EXPECT_TRUE(CallRetrySetHostNow());

  // Simulate verification succeeding.
  fake_host_verifier()->set_is_host_verified(true);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostVerified, test_devices()[0]);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kHostVerified, test_devices()[0],
                          observer.get(), 3u /* expected_observer_index */);

  // Remove the host.
  multidevice_setup()->RemoveHostDevice();
  fake_host_verifier()->set_is_host_verified(false);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      std::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          std::nullopt /* host_device */, observer.get(),
                          4u /* expected_observer_index */);

  // Simulate the host being removed on the back-end.
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(std::nullopt);
}

TEST_F(MultiDeviceSetupImplTest, TestGetEligibleActiveHosts) {
  // Start with no eligible devices.
  EXPECT_TRUE(CallGetEligibleActiveHostDevices().empty());

  multidevice::DeviceWithConnectivityStatusList host_device_list;
  for (auto remote_device_ref : test_devices()) {
    host_device_list.emplace_back(multidevice::DeviceWithConnectivityStatus(
        remote_device_ref, cryptauthv2::ConnectivityStatus::ONLINE));
  }
  // Simulate a sync occurring; now, all of the test devices are eligible hosts.
  fake_eligible_host_devices_provider()->set_eligible_active_host_devices(
      host_device_list);

  std::vector<mojom::HostDevicePtr> result_hosts =
      CallGetEligibleActiveHostDevices();
  for (size_t i = 0; i < kNumTestDevices; i++) {
    EXPECT_EQ(*GetMutableRemoteDevice(test_devices()[i]),
              result_hosts[i]->remote_device);
    EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
              result_hosts[i]->connectivity_status);
  }
}

TEST_F(MultiDeviceSetupImplTest, TestSetHostDevice_InvalidAuthToken) {
  // Start valid eligible host devices.
  fake_eligible_host_devices_provider()->set_eligible_host_devices(
      test_devices());
  EXPECT_EQ(RefListToRawList(test_devices()), CallGetEligibleHostDevices());
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      std::nullopt /* host_device */);

  // Set a valid host as the host device, but pass an invalid token.
  std::string host_id = test_devices()[0].instance_id();
  EXPECT_FALSE(CallSetHostDevice(host_id, "invalidAuthToken"));
  EXPECT_FALSE(fake_host_backend_delegate()->HasPendingHostRequest());
}

TEST_F(MultiDeviceSetupImplTest, TestSetHostDeviceWithoutAuthToken) {
  // Add a status observer.
  auto observer = std::make_unique<FakeHostStatusObserver>();
  multidevice_setup()->AddHostStatusObserver(observer->GenerateRemote());

  // Start valid eligible host devices.
  fake_eligible_host_devices_provider()->set_eligible_host_devices(
      test_devices());
  EXPECT_EQ(RefListToRawList(test_devices()), CallGetEligibleHostDevices());
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      std::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          std::nullopt /* host_device */, observer.get(),
                          0u /* expected_observer_index */);

  // Set a valid host as the host device without an auth token.
  std::string host_id = test_devices()[0].instance_id();
  EXPECT_TRUE(CallSetHostDeviceWithoutAuth(host_id));
  EXPECT_TRUE(fake_host_backend_delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0],
            fake_host_backend_delegate()->GetPendingHostRequest());
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0], observer.get(), 1u /* expected_observer_index */);
}

TEST_F(MultiDeviceSetupImplTest, SetAndGetQuickStartPhoneInstanceID) {
  EXPECT_EQ(std::nullopt, CallGetQuickStartPhoneInstanceID());
  const std::string& expected_qs_phone_instance_id = "qsPhoneInstanceID1";
  multidevice_setup()->SetQuickStartPhoneInstanceID(
      expected_qs_phone_instance_id);
  EXPECT_EQ(expected_qs_phone_instance_id, CallGetQuickStartPhoneInstanceID());
}

}  // namespace multidevice_setup

}  // namespace ash
