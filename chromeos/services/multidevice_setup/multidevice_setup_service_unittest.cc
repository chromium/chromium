// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_observer.h"
#include "chromeos/services/multidevice_setup/fake_host_status_observer.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_auth_token_validator.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"
#include "chromeos/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/fake_gcm_device_info_provider.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 3;

class FakeMultiDeviceSetupFactory : public MultiDeviceSetupImpl::Factory {
 public:
  FakeMultiDeviceSetupFactory(
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      FakeAuthTokenValidator* expected_auth_token_validator,
      OobeCompletionTracker* expected_oobe_completion_tracker,
      FakeAndroidSmsAppHelperDelegate* expected_android_sms_app_helper_delegate,
      FakeAndroidSmsPairingStateTracker*
          expected_android_sms_pairing_state_tracker,
      const cryptauth::FakeGcmDeviceInfoProvider*
          expected_gcm_device_info_provider)
      : expected_testing_pref_service_(expected_testing_pref_service),
        expected_device_sync_client_(expected_device_sync_client),
        expected_auth_token_validator_(expected_auth_token_validator),
        expected_oobe_completion_tracker_(expected_oobe_completion_tracker),
        expected_android_sms_app_helper_delegate_(
            expected_android_sms_app_helper_delegate),
        expected_android_sms_pairing_state_tracker_(
            expected_android_sms_pairing_state_tracker),
        expected_gcm_device_info_provider_(expected_gcm_device_info_provider) {}

  ~FakeMultiDeviceSetupFactory() override = default;

  FakeMultiDeviceSetup* instance() { return instance_; }

 private:
  std::unique_ptr<MultiDeviceSetupBase> BuildInstance(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      AuthTokenValidator* auth_token_validator,
      OobeCompletionTracker* oobe_completion_tracker,
      std::unique_ptr<AndroidSmsAppHelperDelegate>
          android_sms_app_helper_delegate,
      std::unique_ptr<AndroidSmsPairingStateTracker>
          android_sms_pairing_state_tracker,
      const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider)
      override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_auth_token_validator_, auth_token_validator);
    EXPECT_EQ(expected_oobe_completion_tracker_, oobe_completion_tracker);
    EXPECT_EQ(expected_android_sms_app_helper_delegate_,
              android_sms_app_helper_delegate.get());
    EXPECT_EQ(expected_android_sms_pairing_state_tracker_,
              android_sms_pairing_state_tracker.get());
    EXPECT_EQ(expected_gcm_device_info_provider_, gcm_device_info_provider);

    auto instance = std::make_unique<FakeMultiDeviceSetup>();
    instance_ = instance.get();
    return instance;
  }

  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  FakeAuthTokenValidator* expected_auth_token_validator_;
  OobeCompletionTracker* expected_oobe_completion_tracker_;
  FakeAndroidSmsAppHelperDelegate* expected_android_sms_app_helper_delegate_;
  FakeAndroidSmsPairingStateTracker*
      expected_android_sms_pairing_state_tracker_;
  const cryptauth::FakeGcmDeviceInfoProvider*
      expected_gcm_device_info_provider_;

  FakeMultiDeviceSetup* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiDeviceSetupFactory);
};

}  // namespace

class MultiDeviceSetupServiceTest : public testing::Test {
 protected:
  MultiDeviceSetupServiceTest()
      : test_devices_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_auth_token_validator_ = std::make_unique<FakeAuthTokenValidator>();
    fake_oobe_completion_tracker_ = std::make_unique<OobeCompletionTracker>();
    auto fake_android_sms_app_helper_delegate =
        std::make_unique<FakeAndroidSmsAppHelperDelegate>();
    fake_android_sms_app_helper_delegate_ =
        fake_android_sms_app_helper_delegate.get();
    auto fake_android_sms_pairing_state_tracker =
        std::make_unique<FakeAndroidSmsPairingStateTracker>();
    fake_android_sms_pairing_state_tracker_ =
        fake_android_sms_pairing_state_tracker.get();
    fake_gcm_device_info_provider_ =
        std::make_unique<cryptauth::FakeGcmDeviceInfoProvider>(
            cryptauth::GcmDeviceInfo());

    fake_multidevice_setup_factory_ =
        std::make_unique<FakeMultiDeviceSetupFactory>(
            test_pref_service_.get(), fake_device_sync_client_.get(),
            fake_auth_token_validator_.get(),
            fake_oobe_completion_tracker_.get(),
            fake_android_sms_app_helper_delegate_,
            fake_android_sms_pairing_state_tracker_,
            fake_gcm_device_info_provider_.get());
    MultiDeviceSetupImpl::Factory::SetFactoryForTesting(
        fake_multidevice_setup_factory_.get());

    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::make_unique<MultiDeviceSetupService>(
                test_pref_service_.get(), fake_device_sync_client_.get(),
                fake_auth_token_validator_.get(),
                fake_oobe_completion_tracker_.get(),
                std::move(fake_android_sms_app_helper_delegate),
                std::move(fake_android_sms_pairing_state_tracker),
                fake_gcm_device_info_provider_.get()));

    auto connector = connector_factory_->CreateConnector();

    connector->BindInterface(mojom::kServiceName, &multidevice_setup_ptr_);
    multidevice_setup_ptr_.FlushForTesting();

    connector->BindInterface(mojom::kServiceName,
                             &privileged_host_device_setter_ptr_);
    privileged_host_device_setter_ptr_.FlushForTesting();
  }

  void TearDown() override {
    MultiDeviceSetupImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging type) {
    EXPECT_FALSE(last_debug_event_success_);

    base::RunLoop run_loop;
    multidevice_setup_ptr_->TriggerEventForDebugging(
        type,
        base::BindOnce(&MultiDeviceSetupServiceTest::OnDebugEventTriggered,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    // Always expected to fail before initialization completes.
    EXPECT_FALSE(*last_debug_event_success_);
    last_debug_event_success_.reset();
  }

  void FinishInitialization() {
    EXPECT_FALSE(fake_multidevice_setup());
    fake_device_sync_client_->set_local_device_metadata(test_devices_[0]);
    fake_device_sync_client_->NotifyReady();
    EXPECT_TRUE(fake_multidevice_setup());
  }

  FakeMultiDeviceSetup* fake_multidevice_setup() {
    return fake_multidevice_setup_factory_->instance();
  }

  mojom::MultiDeviceSetupPtr& multidevice_setup_ptr() {
    return multidevice_setup_ptr_;
  }

  mojom::PrivilegedHostDeviceSetterPtr& privileged_host_device_setter_ptr() {
    return privileged_host_device_setter_ptr_;
  }

 private:
  void OnDebugEventTriggered(base::OnceClosure quit_closure, bool success) {
    last_debug_event_success_ = success;
    std::move(quit_closure).Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const cryptauth::RemoteDeviceRefList test_devices_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeAuthTokenValidator> fake_auth_token_validator_;
  std::unique_ptr<OobeCompletionTracker> fake_oobe_completion_tracker_;
  FakeAndroidSmsAppHelperDelegate* fake_android_sms_app_helper_delegate_;
  FakeAndroidSmsPairingStateTracker* fake_android_sms_pairing_state_tracker_;
  std::unique_ptr<cryptauth::FakeGcmDeviceInfoProvider>
      fake_gcm_device_info_provider_;

  std::unique_ptr<FakeMultiDeviceSetupFactory> fake_multidevice_setup_factory_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  base::Optional<bool> last_debug_event_success_;

  mojom::MultiDeviceSetupPtr multidevice_setup_ptr_;
  mojom::PrivilegedHostDeviceSetterPtr privileged_host_device_setter_ptr_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupServiceTest);
};

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebuggingBeforeInitialization) {
  CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists);
  CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched);
  CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded);
}

TEST_F(MultiDeviceSetupServiceTest, CallFunctionsBeforeInitialization) {
  // SetAccountStatusChangeDelegate().
  auto fake_account_status_change_delegate =
      std::make_unique<FakeAccountStatusChangeDelegate>();
  multidevice_setup_ptr()->SetAccountStatusChangeDelegate(
      fake_account_status_change_delegate->GenerateInterfacePtr());
  multidevice_setup_ptr().FlushForTesting();

  // AddHostStatusObserver().
  auto fake_host_status_observer = std::make_unique<FakeHostStatusObserver>();
  multidevice_setup_ptr()->AddHostStatusObserver(
      fake_host_status_observer->GenerateInterfacePtr());
  multidevice_setup_ptr().FlushForTesting();

  // AddFeatureStateObserver().
  auto fake_feature_state_observer =
      std::make_unique<FakeFeatureStateObserver>();
  multidevice_setup_ptr()->AddFeatureStateObserver(
      fake_feature_state_observer->GenerateInterfacePtr());
  multidevice_setup_ptr().FlushForTesting();

  // GetEligibleHostDevices().
  multidevice_setup_ptr()->GetEligibleHostDevices(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  // GetHostStatus().
  multidevice_setup_ptr()->GetHostStatus(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  // SetFeatureEnabledState().
  multidevice_setup_ptr()->SetFeatureEnabledState(
      mojom::Feature::kBetterTogetherSuite, true /* enabled */, "authToken",
      base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  // GetFeatureStates().
  multidevice_setup_ptr()->GetFeatureStates(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  // RetrySetHostNow().
  multidevice_setup_ptr()->RetrySetHostNow(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  // None of these requests should have been processed yet, since initialization
  // was not complete.
  EXPECT_FALSE(fake_multidevice_setup());

  // Finish initialization; all of the pending calls should have been forwarded.
  FinishInitialization();
  EXPECT_TRUE(fake_multidevice_setup()->delegate());
  EXPECT_TRUE(fake_multidevice_setup()->HasAtLeastOneHostStatusObserver());
  EXPECT_TRUE(fake_multidevice_setup()->HasAtLeastOneFeatureStateObserver());
  EXPECT_EQ(1u, fake_multidevice_setup()->get_eligible_hosts_args().size());
  EXPECT_EQ(1u, fake_multidevice_setup()->get_host_args().size());
  EXPECT_EQ(1u, fake_multidevice_setup()->set_feature_enabled_args().size());
  EXPECT_EQ(1u, fake_multidevice_setup()->get_feature_states_args().size());
  EXPECT_EQ(1u, fake_multidevice_setup()->retry_set_host_now_args().size());
}

TEST_F(MultiDeviceSetupServiceTest, SetThenRemoveBeforeInitialization) {
  multidevice_setup_ptr()->SetHostDevice("deviceId1", "authToken",
                                         base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  privileged_host_device_setter_ptr()->SetHostDevice("deviceId2",
                                                     base::DoNothing());
  privileged_host_device_setter_ptr().FlushForTesting();

  multidevice_setup_ptr()->RemoveHostDevice();
  multidevice_setup_ptr().FlushForTesting();

  EXPECT_FALSE(fake_multidevice_setup());

  // Finish initialization; since the SetHostDevice() call was followed by a
  // RemoveHostDevice() call, only the RemoveHostDevice() call should have been
  // forwarded.
  FinishInitialization();
  EXPECT_TRUE(fake_multidevice_setup()->set_host_args().empty());
  EXPECT_TRUE(fake_multidevice_setup()->set_host_without_auth_args().empty());
  EXPECT_EQ(1u, fake_multidevice_setup()->num_remove_host_calls());
}

TEST_F(MultiDeviceSetupServiceTest, RemoveThenSetThenSetBeforeInitialization) {
  multidevice_setup_ptr()->RemoveHostDevice();
  multidevice_setup_ptr().FlushForTesting();

  privileged_host_device_setter_ptr()->SetHostDevice("deviceId1",
                                                     base::DoNothing());
  privileged_host_device_setter_ptr().FlushForTesting();

  multidevice_setup_ptr()->SetHostDevice("deviceId2", "authToken2",
                                         base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  multidevice_setup_ptr()->SetHostDevice("deviceId3", "authToken3",
                                         base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  EXPECT_FALSE(fake_multidevice_setup());

  // Finish initialization; only the second SetHostDevice() call should have
  // been forwarded.
  FinishInitialization();
  EXPECT_EQ(0u, fake_multidevice_setup()->num_remove_host_calls());
  EXPECT_TRUE(fake_multidevice_setup()->set_host_without_auth_args().empty());
  EXPECT_EQ(1u, fake_multidevice_setup()->set_host_args().size());
  EXPECT_EQ("deviceId3",
            std::get<0>(fake_multidevice_setup()->set_host_args()[0]));
  EXPECT_EQ("authToken3",
            std::get<1>(fake_multidevice_setup()->set_host_args()[0]));
}

TEST_F(MultiDeviceSetupServiceTest,
       RemoveThenSetThenSetBeforeInitialization_NoAuthToken) {
  multidevice_setup_ptr()->RemoveHostDevice();
  multidevice_setup_ptr().FlushForTesting();

  multidevice_setup_ptr()->SetHostDevice("deviceId1", "authToken1",
                                         base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  multidevice_setup_ptr()->SetHostDevice("deviceId2", "authToken2",
                                         base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();

  privileged_host_device_setter_ptr()->SetHostDevice("deviceId3",
                                                     base::DoNothing());
  privileged_host_device_setter_ptr().FlushForTesting();

  EXPECT_FALSE(fake_multidevice_setup());

  // Finish initialization; only the second SetHostDevice() call should have
  // been forwarded.
  FinishInitialization();
  EXPECT_EQ(0u, fake_multidevice_setup()->num_remove_host_calls());
  EXPECT_TRUE(fake_multidevice_setup()->set_host_args().empty());
  EXPECT_EQ(1u, fake_multidevice_setup()->set_host_without_auth_args().size());
  EXPECT_EQ("deviceId3",
            fake_multidevice_setup()->set_host_without_auth_args()[0].first);
}

TEST_F(MultiDeviceSetupServiceTest, FinishInitializationFirst) {
  // Finish initialization before calling anything; this should result in
  // the calls being forwarded immediately.
  FinishInitialization();

  // SetAccountStatusChangeDelegate().
  auto fake_account_status_change_delegate =
      std::make_unique<FakeAccountStatusChangeDelegate>();
  multidevice_setup_ptr()->SetAccountStatusChangeDelegate(
      fake_account_status_change_delegate->GenerateInterfacePtr());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_TRUE(fake_multidevice_setup()->delegate());

  // AddHostStatusObserver().
  auto fake_host_status_observer = std::make_unique<FakeHostStatusObserver>();
  multidevice_setup_ptr()->AddHostStatusObserver(
      fake_host_status_observer->GenerateInterfacePtr());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_TRUE(fake_multidevice_setup()->HasAtLeastOneHostStatusObserver());

  // AddFeatureStateObserver().
  auto fake_feature_state_observer =
      std::make_unique<FakeFeatureStateObserver>();
  multidevice_setup_ptr()->AddFeatureStateObserver(
      fake_feature_state_observer->GenerateInterfacePtr());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_TRUE(fake_multidevice_setup()->HasAtLeastOneFeatureStateObserver());

  // GetEligibleHostDevices().
  multidevice_setup_ptr()->GetEligibleHostDevices(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->get_eligible_hosts_args().size());

  // SetHostDevice().
  multidevice_setup_ptr()->SetHostDevice("deviceId", "authToken",
                                         base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->set_host_args().size());

  // RemoveHostDevice().
  multidevice_setup_ptr()->RemoveHostDevice();
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->num_remove_host_calls());

  // GetHostStatus().
  multidevice_setup_ptr()->GetHostStatus(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->get_host_args().size());

  // SetFeatureEnabledState().
  multidevice_setup_ptr()->SetFeatureEnabledState(
      mojom::Feature::kBetterTogetherSuite, true /* enabled */, "authToken",
      base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->set_feature_enabled_args().size());

  // GetFeatureStates().
  multidevice_setup_ptr()->GetFeatureStates(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->get_feature_states_args().size());

  // RetrySetHostNow().
  multidevice_setup_ptr()->RetrySetHostNow(base::DoNothing());
  multidevice_setup_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->retry_set_host_now_args().size());

  // SetHostDevice(), without an auth token.
  privileged_host_device_setter_ptr()->SetHostDevice("deviceId",
                                                     base::DoNothing());
  privileged_host_device_setter_ptr().FlushForTesting();
  EXPECT_EQ(1u, fake_multidevice_setup()->set_host_without_auth_args().size());
}

}  // namespace multidevice_setup

}  // namespace chromeos
