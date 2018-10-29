// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_initializer.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 5u;

class FakeMultiDeviceSetupInitializerFactory
    : public MultiDeviceSetupInitializer::Factory {
 public:
  explicit FakeMultiDeviceSetupInitializerFactory(
      std::unique_ptr<FakeMultiDeviceSetup> fake_multidevice_setup)
      : fake_multidevice_setup_(std::move(fake_multidevice_setup)) {}

  ~FakeMultiDeviceSetupInitializerFactory() override = default;

  // MultiDeviceSetupInitializer::Factory:
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
    EXPECT_TRUE(fake_multidevice_setup_);
    return std::move(fake_multidevice_setup_);
  }

 private:
  std::unique_ptr<FakeMultiDeviceSetup> fake_multidevice_setup_;
};

class TestMultiDeviceSetupClientObserver
    : public MultiDeviceSetupClient::Observer {
 public:
  TestMultiDeviceSetupClientObserver() = default;
  ~TestMultiDeviceSetupClientObserver() override = default;

  const std::vector<MultiDeviceSetupClient::HostStatusWithDevice>&
  host_status_updates() const {
    return host_status_updates_;
  }

  const std::vector<MultiDeviceSetupClient::FeatureStatesMap>&
  feature_state_updates() const {
    return feature_state_updates_;
  }

 private:
  // MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(const MultiDeviceSetupClient::HostStatusWithDevice&
                               host_device_with_status) override {
    host_status_updates_.push_back(host_device_with_status);
  }

  void OnFeatureStatesChanged(const MultiDeviceSetupClient::FeatureStatesMap&
                                  feature_states_map) override {
    feature_state_updates_.push_back(feature_states_map);
  }

  std::vector<MultiDeviceSetupClient::HostStatusWithDevice>
      host_status_updates_;
  std::vector<MultiDeviceSetupClient::FeatureStatesMap> feature_state_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestMultiDeviceSetupClientObserver);
};

base::Optional<cryptauth::RemoteDevice> GetRemoteDeviceFromRef(
    const base::Optional<cryptauth::RemoteDeviceRef>& remote_device_ref) {
  if (!remote_device_ref)
    return base::Optional<cryptauth::RemoteDevice>();

  return *cryptauth::GetMutableRemoteDevice(*remote_device_ref);
}

}  // namespace

class MultiDeviceSetupClientImplTest : public testing::Test {
 protected:
  MultiDeviceSetupClientImplTest()
      : test_remote_device_list_(
            cryptauth::CreateRemoteDeviceListForTest(kNumTestDevices)),
        test_remote_device_ref_list_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}

  // testing::Test:
  void SetUp() override {
    auto fake_multidevice_setup = std::make_unique<FakeMultiDeviceSetup>();
    fake_multidevice_setup_ = fake_multidevice_setup.get();
    fake_multidevice_setup_impl_factory_ =
        std::make_unique<FakeMultiDeviceSetupInitializerFactory>(
            std::move(fake_multidevice_setup));
    MultiDeviceSetupInitializer::Factory::SetFactoryForTesting(
        fake_multidevice_setup_impl_factory_.get());

    auto multidevice_setup_service = std::make_unique<MultiDeviceSetupService>(
        nullptr /* pref_service */, nullptr /* device_sync_client */,
        nullptr /* auth_token_validator */,
        nullptr /* oobe_completion_tracker */,
        nullptr /* android_sms_app_helper_delegate */,
        nullptr /* android_sms_pairing_state_tracker */,
        nullptr /* gcm_device_info_provider */);

    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::move(multidevice_setup_service));
    connector_ = connector_factory_->CreateConnector();
  }

  void InitializeClient(
      const MultiDeviceSetupClient::HostStatusWithDevice&
          host_status_with_device =
              MultiDeviceSetupClient::GenerateDefaultHostStatusWithDevice(),
      const MultiDeviceSetupClient::FeatureStatesMap& feature_states_map =
          MultiDeviceSetupClient::GenerateDefaultFeatureStatesMap()) {
    client_ = MultiDeviceSetupClientImpl::Factory::Get()->BuildInstance(
        connector_.get());
    SendPendingMojoMessages();

    // When |client_| is created, it requests the current host status and
    // feature states.
    std::move(fake_multidevice_setup_->get_host_args()[0])
        .Run(host_status_with_device.first,
             GetRemoteDeviceFromRef(host_status_with_device.second));
    std::move(fake_multidevice_setup_->get_feature_states_args()[0])
        .Run(feature_states_map);
    fake_multidevice_setup_->FlushForTesting();

    test_observer_ = std::make_unique<TestMultiDeviceSetupClientObserver>();
    client_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    MultiDeviceSetupInitializer::Factory::SetFactoryForTesting(nullptr);
    client_->RemoveObserver(test_observer_.get());
  }

  void SimulateHostStatusChange(
      const MultiDeviceSetupClient::HostStatusWithDevice&
          host_status_with_device) {
    size_t initial_results_size = test_observer_->host_status_updates().size();

    fake_multidevice_setup_->NotifyHostStatusChanged(
        host_status_with_device.first,
        GetRemoteDeviceFromRef(host_status_with_device.second));
    fake_multidevice_setup_->FlushForTesting();

    EXPECT_EQ(initial_results_size + 1,
              test_observer_->host_status_updates().size());
    EXPECT_EQ(
        host_status_with_device.first,
        test_observer_->host_status_updates()[initial_results_size].first);
    EXPECT_EQ(
        host_status_with_device.second,
        test_observer_->host_status_updates()[initial_results_size].second);
  }

  void SimulateFeatureStatesChange(
      const MultiDeviceSetupClient::FeatureStatesMap& feature_states_map) {
    size_t initial_results_size =
        test_observer_->feature_state_updates().size();

    fake_multidevice_setup_->NotifyFeatureStateChanged(feature_states_map);
    fake_multidevice_setup_->FlushForTesting();

    EXPECT_EQ(initial_results_size + 1,
              test_observer_->feature_state_updates().size());
    EXPECT_EQ(feature_states_map,
              test_observer_->feature_state_updates()[initial_results_size]);
  }

  void CallGetEligibleHostDevices(
      const cryptauth::RemoteDeviceList& expected_eligible_host_devices) {
    base::RunLoop run_loop;

    client_->GetEligibleHostDevices(base::BindOnce(
        &MultiDeviceSetupClientImplTest::OnGetEligibleHostDevicesCompleted,
        base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    std::vector<mojom::MultiDeviceSetup::GetEligibleHostDevicesCallback>&
        callbacks = fake_multidevice_setup_->get_eligible_hosts_args();
    EXPECT_EQ(1u, callbacks.size());
    std::move(callbacks[0]).Run(expected_eligible_host_devices);

    run_loop.Run();

    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        *eligible_host_devices_, expected_eligible_host_devices);
  }

  void CallSetHostDevice(const std::string& public_key,
                         const std::string& auth_token,
                         bool expect_success) {
    base::RunLoop run_loop;

    client_->SetHostDevice(
        public_key, auth_token,
        base::BindOnce(
            &MultiDeviceSetupClientImplTest::OnSetHostDeviceCompleted,
            base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    std::vector<std::tuple<std::string, std::string,
                           mojom::MultiDeviceSetup::SetHostDeviceCallback>>&
        callbacks = fake_multidevice_setup_->set_host_args();
    EXPECT_EQ(1u, callbacks.size());
    EXPECT_EQ(public_key, std::get<0>(callbacks[0]));
    EXPECT_EQ(auth_token, std::get<1>(callbacks[0]));
    std::move(std::get<2>(callbacks[0])).Run(expect_success);

    run_loop.Run();

    EXPECT_EQ(expect_success, set_host_device_success_);
  }

  void CallRemoveHostDevice() {
    EXPECT_EQ(0u, fake_multidevice_setup_->num_remove_host_calls());

    client_->RemoveHostDevice();

    SendPendingMojoMessages();

    EXPECT_EQ(1u, fake_multidevice_setup_->num_remove_host_calls());
  }

  void CallSetFeatureEnabledState(mojom::Feature feature,
                                  bool enabled,
                                  const base::Optional<std::string>& auth_token,
                                  bool should_succeed) {
    size_t num_set_feature_enabled_args_before_call =
        fake_multidevice_setup_->set_feature_enabled_args().size();

    base::RunLoop run_loop;
    client_->SetFeatureEnabledState(
        feature, enabled, auth_token,
        base::BindOnce(
            &MultiDeviceSetupClientImplTest::OnSetFeatureEnabledStateCompleted,
            base::Unretained(this), run_loop.QuitClosure()));
    SendPendingMojoMessages();

    EXPECT_EQ(num_set_feature_enabled_args_before_call + 1u,
              fake_multidevice_setup_->set_feature_enabled_args().size());
    EXPECT_EQ(feature,
              std::get<0>(
                  fake_multidevice_setup_->set_feature_enabled_args().back()));
    EXPECT_EQ(enabled,
              std::get<1>(
                  fake_multidevice_setup_->set_feature_enabled_args().back()));
    EXPECT_EQ(auth_token,
              std::get<2>(
                  fake_multidevice_setup_->set_feature_enabled_args().back()));
    std::move(
        std::get<3>(fake_multidevice_setup_->set_feature_enabled_args().back()))
        .Run(should_succeed /* success */);

    run_loop.Run();
    EXPECT_EQ(should_succeed, *set_feature_enabled_state_success_);
  }

  void CallRetrySetHostNow(bool expect_success) {
    base::RunLoop run_loop;

    client_->RetrySetHostNow(base::BindOnce(
        &MultiDeviceSetupClientImplTest::OnRetrySetHostNowCompleted,
        base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    std::vector<mojom::MultiDeviceSetup::RetrySetHostNowCallback>& callbacks =
        fake_multidevice_setup_->retry_set_host_now_args();
    EXPECT_EQ(1u, callbacks.size());
    std::move(callbacks[0]).Run(expect_success);

    run_loop.Run();

    EXPECT_EQ(expect_success, retry_set_host_now_success_);
  }

  void CallTriggerEventForDebugging(mojom::EventTypeForDebugging type,
                                    bool expect_success) {
    base::RunLoop run_loop;

    client_->TriggerEventForDebugging(
        type, base::BindOnce(&MultiDeviceSetupClientImplTest::
                                 OnTriggerEventForDebuggingCompleted,
                             base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    std::vector<
        std::pair<mojom::EventTypeForDebugging,
                  mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback>>&
        callbacks = fake_multidevice_setup_->triggered_debug_events();
    EXPECT_EQ(1u, callbacks.size());
    EXPECT_EQ(type, callbacks[0].first);
    std::move(callbacks[0].second).Run(expect_success);

    run_loop.Run();

    EXPECT_EQ(expect_success, trigger_event_for_debugging_success_);
  }

  MultiDeviceSetupClient* client() { return client_.get(); }

  cryptauth::RemoteDeviceList test_remote_device_list_;
  const cryptauth::RemoteDeviceRefList test_remote_device_ref_list_;
  std::unique_ptr<TestMultiDeviceSetupClientObserver> test_observer_;

 private:
  void SendPendingMojoMessages() {
    static_cast<MultiDeviceSetupClientImpl*>(client_.get())->FlushForTesting();
  }

  // MultiDeviceSetupClientImpl cached its devices in a RemoteDeviceCache, which
  // stores devices in an unordered_map -- retrieved devices thus need to be
  // sorted before comparison.
  void VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      cryptauth::RemoteDeviceRefList remote_device_ref_list,
      cryptauth::RemoteDeviceList remote_device_list) {
    std::sort(remote_device_list.begin(), remote_device_list.end());
    std::sort(remote_device_ref_list.begin(), remote_device_ref_list.end());

    EXPECT_EQ(remote_device_list.size(), remote_device_ref_list.size());
    for (size_t i = 0; i < remote_device_list.size(); ++i) {
      EXPECT_EQ(remote_device_list[i].public_key,
                remote_device_ref_list[i].public_key());
    }
  }

  void OnGetEligibleHostDevicesCompleted(
      base::OnceClosure quit_closure,
      const cryptauth::RemoteDeviceRefList& eligible_host_devices) {
    eligible_host_devices_ = eligible_host_devices;
    std::move(quit_closure).Run();
  }

  void OnSetHostDeviceCompleted(base::OnceClosure quit_closure, bool success) {
    set_host_device_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnGetHostStatusCompleted(
      base::OnceClosure quit_closure,
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
    get_host_status_result_ = std::make_pair(host_status, host_device);
    std::move(quit_closure).Run();
  }

  void OnSetFeatureEnabledStateCompleted(base::OnceClosure quit_closure,
                                         bool success) {
    set_feature_enabled_state_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnGetFeatureStatesCompleted(
      base::OnceClosure quit_closure,
      const base::flat_map<mojom::Feature, mojom::FeatureState>&
          feature_states_map) {
    get_feature_states_result_ = feature_states_map;
    std::move(quit_closure).Run();
  }

  void OnRetrySetHostNowCompleted(base::OnceClosure quit_closure,
                                  bool success) {
    retry_set_host_now_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnTriggerEventForDebuggingCompleted(base::OnceClosure quit_closure,
                                           bool success) {
    trigger_event_for_debugging_success_ = success;
    std::move(quit_closure).Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  FakeMultiDeviceSetup* fake_multidevice_setup_;
  std::unique_ptr<FakeMultiDeviceSetupInitializerFactory>
      fake_multidevice_setup_impl_factory_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<MultiDeviceSetupClient> client_;

  base::Optional<cryptauth::RemoteDeviceRefList> eligible_host_devices_;
  base::Optional<bool> set_host_device_success_;
  base::Optional<
      std::pair<mojom::HostStatus, base::Optional<cryptauth::RemoteDeviceRef>>>
      get_host_status_result_;
  base::Optional<bool> set_feature_enabled_state_success_;
  base::Optional<base::flat_map<mojom::Feature, mojom::FeatureState>>
      get_feature_states_result_;
  base::Optional<bool> retry_set_host_now_success_;
  base::Optional<bool> trigger_event_for_debugging_success_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupClientImplTest);
};

TEST_F(MultiDeviceSetupClientImplTest, GetHostStatus) {
  InitializeClient();
  EXPECT_TRUE(test_observer_->host_status_updates().empty());

  MultiDeviceSetupClient::HostStatusWithDevice host_status_with_device =
      std::make_pair(mojom::HostStatus::kNoEligibleHosts,
                     base::nullopt /* expected_host_device */);
  SimulateHostStatusChange(host_status_with_device);
  EXPECT_EQ(host_status_with_device, client()->GetHostStatus());

  host_status_with_device = std::make_pair(mojom::HostStatus::kHostVerified,
                                           test_remote_device_ref_list_[0]);
  SimulateHostStatusChange(host_status_with_device);
  EXPECT_EQ(host_status_with_device, client()->GetHostStatus());
}

TEST_F(MultiDeviceSetupClientImplTest, TestGetEligibleHostDevices) {
  InitializeClient();
  CallGetEligibleHostDevices(test_remote_device_list_);
}

TEST_F(MultiDeviceSetupClientImplTest, TestSetHostDevice_Success) {
  InitializeClient();
  CallSetHostDevice(test_remote_device_list_[0].public_key, "authToken",
                    true /* expect_success */);
}

TEST_F(MultiDeviceSetupClientImplTest, TestSetHostDevice_Failure) {
  InitializeClient();
  CallSetHostDevice(test_remote_device_list_[0].public_key, "authToken",
                    false /* expect_success */);
}

TEST_F(MultiDeviceSetupClientImplTest, TestRemoveHostDevice) {
  InitializeClient();
  CallRemoveHostDevice();
}

TEST_F(MultiDeviceSetupClientImplTest, InitializeWithValues) {
  MultiDeviceSetupClient::HostStatusWithDevice initial_host_status_with_device =
      std::make_pair(mojom::HostStatus::kHostVerified,
                     test_remote_device_ref_list_[0]);

  MultiDeviceSetupClient::FeatureStatesMap initial_feature_states_map{
      {mojom::Feature::kBetterTogetherSuite,
       mojom::FeatureState::kEnabledByUser},
      {mojom::Feature::kInstantTethering, mojom::FeatureState::kEnabledByUser},
      {mojom::Feature::kMessages, mojom::FeatureState::kEnabledByUser},
      {mojom::Feature::kSmartLock, mojom::FeatureState::kEnabledByUser}};

  InitializeClient(initial_host_status_with_device, initial_feature_states_map);
  EXPECT_EQ(initial_host_status_with_device, client()->GetHostStatus());
  EXPECT_EQ(initial_feature_states_map, client()->GetFeatureStates());
}

TEST_F(MultiDeviceSetupClientImplTest, SetFeatureEnabledState) {
  InitializeClient();
  CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                             true /* enabled */, "authToken1",
                             true /* should_succeed */);
  CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                             false /* enabled */, "authToken2",
                             false /* should_succeed */);
  CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                             false /* enabled */, "authToken3",
                             true /* should_succeed */);
}

TEST_F(MultiDeviceSetupClientImplTest, GetFeatureState) {
  InitializeClient();

  MultiDeviceSetupClient::FeatureStatesMap feature_states_map{
      {mojom::Feature::kBetterTogetherSuite,
       mojom::FeatureState::kEnabledByUser},
      {mojom::Feature::kInstantTethering, mojom::FeatureState::kEnabledByUser},
      {mojom::Feature::kMessages, mojom::FeatureState::kEnabledByUser},
      {mojom::Feature::kSmartLock, mojom::FeatureState::kEnabledByUser}};
  SimulateFeatureStatesChange(feature_states_map);
  EXPECT_EQ(feature_states_map, client()->GetFeatureStates());
}

TEST_F(MultiDeviceSetupClientImplTest, TestRetrySetHostNow_Success) {
  InitializeClient();
  CallRetrySetHostNow(true /* expect_success */);
}

TEST_F(MultiDeviceSetupClientImplTest, TestRetrySetHostNow_Failure) {
  InitializeClient();
  CallRetrySetHostNow(false /* expect_success */);
}

TEST_F(MultiDeviceSetupClientImplTest, TestTriggerEventForDebugging_Success) {
  InitializeClient();
  CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists,
      true /* expect_success */);
}

TEST_F(MultiDeviceSetupClientImplTest, TestTriggerEventForDebugging_Failure) {
  InitializeClient();
  CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists,
      false /* expect_success */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
