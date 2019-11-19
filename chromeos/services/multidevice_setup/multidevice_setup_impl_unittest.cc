// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/device_sync/public/cpp/fake_gcm_device_info_provider.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/services/multidevice_setup/android_sms_app_installing_status_observer.h"
#include "chromeos/services/multidevice_setup/device_reenroller.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate_notifier.h"
#include "chromeos/services/multidevice_setup/fake_eligible_host_devices_provider.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_observer.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "chromeos/services/multidevice_setup/fake_host_device_timestamp_manager.h"
#include "chromeos/services/multidevice_setup/fake_host_status_observer.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/fake_host_verifier.h"
#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"
#include "chromeos/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_device_timestamp_manager_impl.h"
#include "chromeos/services/multidevice_setup/host_status_provider_impl.h"
#include "chromeos/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_auth_token_validator.h"
#include "chromeos/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 3;

const char kValidAuthToken[] = "validAuthToken";

multidevice::RemoteDeviceList RefListToRawList(
    const multidevice::RemoteDeviceRefList& ref_list) {
  multidevice::RemoteDeviceList raw_list;
  std::transform(ref_list.begin(), ref_list.end(), std::back_inserter(raw_list),
                 [](const multidevice::RemoteDeviceRef ref) {
                   return *GetMutableRemoteDevice(ref);
                 });
  return raw_list;
}

base::Optional<multidevice::RemoteDevice> RefToRaw(
    const base::Optional<multidevice::RemoteDeviceRef>& ref) {
  if (!ref)
    return base::nullopt;

  return *GetMutableRemoteDevice(*ref);
}

class FakeEligibleHostDevicesProviderFactory
    : public EligibleHostDevicesProviderImpl::Factory {
 public:
  FakeEligibleHostDevicesProviderFactory(
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : expected_device_sync_client_(expected_device_sync_client) {}

  ~FakeEligibleHostDevicesProviderFactory() override = default;

  FakeEligibleHostDevicesProvider* instance() { return instance_; }

 private:
  // EligibleHostDevicesProviderImpl::Factory:
  std::unique_ptr<EligibleHostDevicesProvider> BuildInstance(
      device_sync::DeviceSyncClient* device_sync_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeEligibleHostDevicesProvider>();
    instance_ = instance.get();
    return instance;
  }

  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;

  FakeEligibleHostDevicesProvider* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeEligibleHostDevicesProviderFactory);
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

  ~FakeHostBackendDelegateFactory() override = default;

  FakeHostBackendDelegate* instance() { return instance_; }

 private:
  // HostBackendDelegateImpl::Factory:
  std::unique_ptr<HostBackendDelegate> BuildInstance(
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

  FakeEligibleHostDevicesProviderFactory*
      fake_eligible_host_devices_provider_factory_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;

  FakeHostBackendDelegate* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostBackendDelegateFactory);
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

  ~FakeHostVerifierFactory() override = default;

  FakeHostVerifier* instance() { return instance_; }

 private:
  // HostVerifierImpl::Factory:
  std::unique_ptr<HostVerifier> BuildInstance(
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

  FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;

  FakeHostVerifier* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostVerifierFactory);
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

  ~FakeHostStatusProviderFactory() override = default;

  FakeHostStatusProvider* instance() { return instance_; }

 private:
  // HostStatusProviderImpl::Factory:
  std::unique_ptr<HostStatusProvider> BuildInstance(
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

  FakeEligibleHostDevicesProviderFactory*
      fake_eligible_host_devices_provider_factory_;
  FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory_;
  FakeHostVerifierFactory* fake_host_verifier_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;

  FakeHostStatusProvider* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostStatusProviderFactory);
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

  ~FakeGrandfatheredEasyUnlockHostDisablerFactory() override = default;

 private:
  // GrandfatheredEasyUnlockHostDisabler::Factory:
  std::unique_ptr<GrandfatheredEasyUnlockHostDisabler> BuildInstance(
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

  FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeGrandfatheredEasyUnlockHostDisablerFactory);
};

class FakeFeatureStateManagerFactory : public FeatureStateManagerImpl::Factory {
 public:
  FakeFeatureStateManagerFactory(
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      FakeAndroidSmsPairingStateTracker*
          expected_android_sms_pairing_state_tracker)
      : expected_testing_pref_service_(expected_testing_pref_service),
        fake_host_status_provider_factory_(fake_host_status_provider_factory),
        expected_device_sync_client_(expected_device_sync_client),
        expected_android_sms_pairing_state_tracker_(
            expected_android_sms_pairing_state_tracker) {}

  ~FakeFeatureStateManagerFactory() override = default;

  FakeFeatureStateManager* instance() { return instance_; }

 private:
  // FeatureStateManagerImpl::Factory:
  std::unique_ptr<FeatureStateManager> BuildInstance(
      PrefService* pref_service,
      HostStatusProvider* host_status_provider,
      device_sync::DeviceSyncClient* device_sync_client,
      AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker)
      override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_android_sms_pairing_state_tracker_,
              android_sms_pairing_state_tracker);

    auto instance = std::make_unique<FakeFeatureStateManager>();
    instance_ = instance.get();
    return instance;
  }

  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  FakeHostStatusProviderFactory* fake_host_status_provider_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  FakeAndroidSmsPairingStateTracker*
      expected_android_sms_pairing_state_tracker_;

  FakeFeatureStateManager* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeFeatureStateManagerFactory);
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

  ~FakeHostDeviceTimestampManagerFactory() override = default;

  FakeHostDeviceTimestampManager* instance() { return instance_; }

 private:
  // HostDeviceTimestampManagerImpl::Factory:
  std::unique_ptr<HostDeviceTimestampManager> BuildInstance(
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

  FakeHostStatusProviderFactory* fake_host_status_provider_factory_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;

  FakeHostDeviceTimestampManager* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostDeviceTimestampManagerFactory);
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

  ~FakeAccountStatusChangeDelegateNotifierFactory() override = default;

  FakeAccountStatusChangeDelegateNotifier* instance() { return instance_; }

 private:
  // AccountStatusChangeDelegateNotifierImpl::Factory:
  std::unique_ptr<AccountStatusChangeDelegateNotifier> BuildInstance(
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

  FakeHostStatusProviderFactory* fake_host_status_provider_factory_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  FakeHostDeviceTimestampManagerFactory*
      fake_host_device_timestamp_manager_factory_;
  OobeCompletionTracker* expected_oobe_completion_tracker_;

  FakeAccountStatusChangeDelegateNotifier* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeAccountStatusChangeDelegateNotifierFactory);
};

class FakeDeviceReenrollerFactory : public DeviceReenroller::Factory {
 public:
  FakeDeviceReenrollerFactory(
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      const device_sync::FakeGcmDeviceInfoProvider*
          expected_gcm_device_info_provider)
      : expected_device_sync_client_(expected_device_sync_client),
        expected_gcm_device_info_provider_(expected_gcm_device_info_provider) {}

  ~FakeDeviceReenrollerFactory() override = default;

 private:
  // DeviceReenroller::Factory:
  std::unique_ptr<DeviceReenroller> BuildInstance(
      device_sync::DeviceSyncClient* device_sync_client,
      const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
      std::unique_ptr<base::OneShotTimer> timer) override {
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_gcm_device_info_provider_, gcm_device_info_provider);
    // Only check inputs and return nullptr. We do not want to trigger the
    // DeviceReenroller logic in these unit tests.
    return nullptr;
  }

  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  const device_sync::GcmDeviceInfoProvider* expected_gcm_device_info_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceReenrollerFactory);
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

  ~FakeAndroidSmsAppInstallingStatusObserverFactory() override = default;

 private:
  // AndroidSmsAppInstallingStatusObserver::Factory:
  std::unique_ptr<AndroidSmsAppInstallingStatusObserver> BuildInstance(
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

  FakeHostStatusProviderFactory* fake_host_status_provider_factory_;
  FakeFeatureStateManagerFactory* fake_feature_state_manager_factory_;
  AndroidSmsAppHelperDelegate* expected_android_sms_app_helper_delegate_;

  DISALLOW_COPY_AND_ASSIGN(FakeAndroidSmsAppInstallingStatusObserverFactory);
};

}  // namespace

class MultiDeviceSetupImplTest : public testing::Test {
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
            fake_android_sms_pairing_state_tracker_.get());
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

    fake_device_reenroller_factory_ =
        std::make_unique<FakeDeviceReenrollerFactory>(
            fake_device_sync_client_.get(),
            fake_gcm_device_info_provider_.get());
    DeviceReenroller::Factory::SetFactoryForTesting(
        fake_device_reenroller_factory_.get());

    fake_android_sms_app_installing_status_observer_factory_ =
        std::make_unique<FakeAndroidSmsAppInstallingStatusObserverFactory>(
            fake_host_status_provider_factory_.get(),
            fake_feature_state_manager_factory_.get(),
            fake_android_sms_app_helper_delegate_.get());
    AndroidSmsAppInstallingStatusObserver::Factory::SetFactoryForTesting(
        fake_android_sms_app_installing_status_observer_factory_.get());

    multidevice_setup_ = MultiDeviceSetupImpl::Factory::Get()->BuildInstance(
        test_pref_service_.get(), fake_device_sync_client_.get(),
        fake_auth_token_validator_.get(), fake_oobe_completion_tracker_.get(),
        fake_android_sms_app_helper_delegate_.get(),
        fake_android_sms_pairing_state_tracker_.get(),
        fake_gcm_device_info_provider_.get());
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
    DeviceReenroller::Factory::SetFactoryForTesting(nullptr);
    AndroidSmsAppInstallingStatusObserver::Factory::SetFactoryForTesting(
        nullptr);
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

  bool CallSetHostDevice(const std::string& host_device_id,
                         const std::string& auth_token) {
    base::RunLoop run_loop;
    multidevice_setup_->SetHostDevice(
        host_device_id, auth_token,
        base::BindOnce(&MultiDeviceSetupImplTest::OnSetHostDeviceResult,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_set_host_success_;
    last_set_host_success_.reset();

    return success;
  }

  bool CallSetHostDeviceWithoutAuth(const std::string& host_device_id) {
    base::RunLoop run_loop;
    multidevice_setup_->SetHostDeviceWithoutAuthToken(
        host_device_id,
        base::BindOnce(
            &MultiDeviceSetupImplTest::OnSetHostDeviceWithoutAuthResult,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_set_host_without_auth_success_;
    last_set_host_without_auth_success_.reset();

    return success;
  }

  std::pair<mojom::HostStatus, base::Optional<multidevice::RemoteDevice>>
  CallGetHostStatus() {
    base::RunLoop run_loop;
    multidevice_setup_->GetHostStatus(
        base::BindOnce(&MultiDeviceSetupImplTest::OnHostStatusReceived,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    std::pair<mojom::HostStatus, base::Optional<multidevice::RemoteDevice>>
        host_status_update = *last_host_status_;
    last_host_status_.reset();

    return host_status_update;
  }

  bool CallSetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      const base::Optional<std::string>& auth_token) {
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

  void VerifyCurrentHostStatus(
      mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDeviceRef>& host_device,
      FakeHostStatusObserver* observer = nullptr,
      size_t expected_observer_index = 0u) {
    std::pair<mojom::HostStatus, base::Optional<multidevice::RemoteDevice>>
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
      const base::Optional<multidevice::RemoteDevice>& host_device) {
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

  base::test::TaskEnvironment task_environment_;

  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeAuthTokenValidator> fake_auth_token_validator_;
  std::unique_ptr<OobeCompletionTracker> fake_oobe_completion_tracker_;
  std::unique_ptr<device_sync::FakeGcmDeviceInfoProvider>
      fake_gcm_device_info_provider_;

  std::unique_ptr<FakeEligibleHostDevicesProviderFactory>
      fake_eligible_host_devices_provider_factory_;
  std::unique_ptr<FakeHostBackendDelegateFactory>
      fake_host_backend_delegate_factory_;
  std::unique_ptr<FakeHostVerifierFactory> fake_host_verifier_factory_;
  std::unique_ptr<FakeHostStatusProviderFactory>
      fake_host_status_provider_factory_;
  std::unique_ptr<FakeGrandfatheredEasyUnlockHostDisablerFactory>
      fake_grandfathered_easy_unlock_host_disabler_factory_;
  std::unique_ptr<FakeFeatureStateManagerFactory>
      fake_feature_state_manager_factory_;
  std::unique_ptr<FakeHostDeviceTimestampManagerFactory>
      fake_host_device_timestamp_manager_factory_;
  std::unique_ptr<FakeAccountStatusChangeDelegateNotifierFactory>
      fake_account_status_change_delegate_notifier_factory_;
  std::unique_ptr<FakeDeviceReenrollerFactory> fake_device_reenroller_factory_;
  std::unique_ptr<FakeAndroidSmsAppInstallingStatusObserverFactory>
      fake_android_sms_app_installing_status_observer_factory_;
  std::unique_ptr<FakeAndroidSmsAppHelperDelegate>
      fake_android_sms_app_helper_delegate_;
  std::unique_ptr<FakeAndroidSmsPairingStateTracker>
      fake_android_sms_pairing_state_tracker_;

  std::unique_ptr<FakeAccountStatusChangeDelegate>
      fake_account_status_change_delegate_;

  base::Optional<bool> last_debug_event_success_;
  base::Optional<multidevice::RemoteDeviceList> last_eligible_devices_list_;
  base::Optional<std::vector<mojom::HostDevicePtr>>
      last_eligible_active_devices_list_;
  base::Optional<bool> last_set_host_success_;
  base::Optional<bool> last_set_host_without_auth_success_;
  base::Optional<
      std::pair<mojom::HostStatus, base::Optional<multidevice::RemoteDevice>>>
      last_host_status_;
  base::Optional<bool> last_set_feature_enabled_state_success_;
  base::Optional<base::flat_map<mojom::Feature, mojom::FeatureState>>
      last_get_feature_states_result_;
  base::Optional<bool> last_retry_success_;

  std::unique_ptr<MultiDeviceSetupBase> multidevice_setup_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupImplTest);
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

  EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
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
                                         base::nullopt /* auth_token */));
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

  EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
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
                                         base::nullopt /* auth_token */));
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

  EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
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
                                         base::nullopt /* auth_token */));
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
                                         base::nullopt /* auth_token */));
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
                          base::nullopt /* host_device */);

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
      base::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          base::nullopt /* host_device */, observer.get(),
                          0u /* expected_observer_index */);

  // There are eligible hosts, but none is set; thus, cannot retry.
  EXPECT_FALSE(CallRetrySetHostNow());

  // Set an invalid host as the host device; this should fail.
  EXPECT_FALSE(CallSetHostDevice("invalidHostDeviceId", kValidAuthToken));
  EXPECT_FALSE(fake_host_backend_delegate()->HasPendingHostRequest());

  // Set device 0 as the host; this should succeed.
  EXPECT_TRUE(
      CallSetHostDevice(test_devices()[0].GetDeviceId(), kValidAuthToken));
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
      base::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          base::nullopt /* host_device */, observer.get(),
                          4u /* expected_observer_index */);

  // Simulate the host being removed on the back-end.
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(base::nullopt);
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
      base::nullopt /* host_device */);

  // Set a valid host as the host device, but pass an invalid token.
  EXPECT_FALSE(
      CallSetHostDevice(test_devices()[0].GetDeviceId(), "invalidAuthToken"));
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
      base::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          base::nullopt /* host_device */, observer.get(),
                          0u /* expected_observer_index */);

  // Set a valid host as the host device, but pass an invalid token.
  EXPECT_TRUE(CallSetHostDeviceWithoutAuth(test_devices()[0].GetDeviceId()));
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

}  // namespace multidevice_setup

}  // namespace chromeos
