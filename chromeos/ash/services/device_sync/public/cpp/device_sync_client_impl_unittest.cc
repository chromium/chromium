// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client_impl.h"

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/device_sync_impl.h"
#include "chromeos/ash/services/device_sync/fake_device_sync.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_prefs.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_client_app_metadata_provider.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_gcm_device_info_provider.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kTestEmail[] = "example@gmail.com";
const char kTestGcmDeviceInfoLongDeviceId[] = "longDeviceId";
const size_t kNumTestDevices = 5u;

const cryptauth::GcmDeviceInfo& GetTestGcmDeviceInfo() {
  static const base::NoDestructor<cryptauth::GcmDeviceInfo> gcm_device_info([] {
    cryptauth::GcmDeviceInfo gcm_device_info;
    gcm_device_info.set_long_device_id(kTestGcmDeviceInfoLongDeviceId);
    return gcm_device_info;
  }());

  return *gcm_device_info;
}

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;
  MOCK_METHOD(instance_id::InstanceID*,
              GetInstanceID,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, RemoveInstanceID, (const std::string& app_id), (override));
  MOCK_METHOD(bool,
              ExistsInstanceID,
              (const std::string& app_id),
              (const override));
};

class FakeDeviceSyncImplFactory : public DeviceSyncImpl::Factory {
 public:
  explicit FakeDeviceSyncImplFactory(
      std::unique_ptr<FakeDeviceSync> fake_device_sync)
      : fake_device_sync_(std::move(fake_device_sync)) {}

  ~FakeDeviceSyncImplFactory() override = default;

  // DeviceSyncImpl::Factory:
  std::unique_ptr<DeviceSyncBase> CreateInstance(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      PrefService* profile_prefs,
      const GcmDeviceInfoProvider* gcm_device_info_provider,
      ClientAppMetadataProvider* client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<base::OneShotTimer> timer,
      AttestationCertificatesSyncer::GetAttestationCertificatesFunction
          get_attestation_certificates_function) override {
    EXPECT_TRUE(fake_device_sync_);
    return std::move(fake_device_sync_);
  }

 private:
  std::unique_ptr<FakeDeviceSync> fake_device_sync_;
};

class TestDeviceSyncClientObserver : public DeviceSyncClient::Observer {
 public:
  ~TestDeviceSyncClientObserver() override = default;

  void set_closure_for_enrollment_finished(base::OnceClosure closure) {
    EXPECT_FALSE(closure_for_enrollment_finished_);
    closure_for_enrollment_finished_ = std::move(closure);
  }

  void set_closure_for_new_devices_synced(base::OnceClosure closure) {
    EXPECT_FALSE(closure_for_new_devices_synced_);
    closure_for_new_devices_synced_ = std::move(closure);
  }

  void OnReady() override {
    if (ready_count_ == 0u) {
      // Ensure that OnReady() was called before the other callbacks.
      EXPECT_FALSE(enrollment_finished_count_);
      EXPECT_FALSE(new_devices_synced_count_);
    }

    ++ready_count_;
  }

  void OnEnrollmentFinished() override {
    // Ensure that OnReady() was called before the other callbacks.
    EXPECT_TRUE(ready_count_);

    ++enrollment_finished_count_;

    EXPECT_TRUE(closure_for_enrollment_finished_);
    std::move(closure_for_enrollment_finished_).Run();
  }

  void OnNewDevicesSynced() override {
    // Ensure that OnReady() was called before the other callbacks.
    EXPECT_TRUE(ready_count_);

    ++new_devices_synced_count_;

    EXPECT_TRUE(closure_for_new_devices_synced_);
    std::move(closure_for_new_devices_synced_).Run();
  }

  size_t ready_count() { return ready_count_; }
  size_t enrollment_finished_count() { return enrollment_finished_count_; }
  size_t new_devices_synced_count() { return new_devices_synced_count_; }

 private:
  size_t ready_count_ = 0u;
  size_t enrollment_finished_count_ = 0u;
  size_t new_devices_synced_count_ = 0u;

  base::OnceClosure closure_for_enrollment_finished_;
  base::OnceClosure closure_for_new_devices_synced_;
};

}  // namespace

class DeviceSyncClientImplTest : public testing::Test {
 public:
  DeviceSyncClientImplTest(const DeviceSyncClientImplTest&) = delete;
  DeviceSyncClientImplTest& operator=(const DeviceSyncClientImplTest&) = delete;

 protected:
  DeviceSyncClientImplTest()
      : test_remote_device_list_(
            multidevice::CreateRemoteDeviceListForTest(kNumTestDevices)),
        test_remote_device_ref_list_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}

  // testing::Test:
  void SetUp() override {
    fake_gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();
    fake_gcm_device_info_provider_ =
        std::make_unique<FakeGcmDeviceInfoProvider>(GetTestGcmDeviceInfo());
    fake_client_app_metadata_provider_ =
        std::make_unique<FakeClientAppMetadataProvider>();

    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    // ConsentLevel::kSignin because this feature is not tied to browser sync
    // consent.
    identity_test_environment_->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);

    auto fake_device_sync = std::make_unique<FakeDeviceSync>();
    fake_device_sync_ = fake_device_sync.get();
    fake_device_sync_impl_factory_ =
        std::make_unique<FakeDeviceSyncImplFactory>(
            std::move(fake_device_sync));
    DeviceSyncImpl::Factory::SetCustomFactory(
        fake_device_sync_impl_factory_.get());

    auto shared_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));

    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterProfilePrefs(test_pref_service_->registry());

    device_sync_ = DeviceSyncImpl::Factory::Create(
        identity_test_environment_->identity_manager(), fake_gcm_driver_.get(),
        &fake_instance_id_driver_, test_pref_service_.get(),
        fake_gcm_device_info_provider_.get(),
        fake_client_app_metadata_provider_.get(), shared_url_loader_factory,
        std::make_unique<base::OneShotTimer>(),
        base::BindRepeating(
            [](AttestationCertificatesSyncer::NotifyCallback notifyCallback,
               const std::string&) {}));

    test_observer_ = std::make_unique<TestDeviceSyncClientObserver>();

    // DeviceSyncClient initialization posts two tasks to the TaskRunner. Idle
    // the TaskRunner so that the tasks can be run via a RunLoop later on.
    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    client_ = std::make_unique<DeviceSyncClientImpl>();
    device_sync_->BindReceiver(
        client_->GetDeviceSyncRemote()->BindNewPipeAndPassReceiver());
    client_->Initialize(test_task_runner);
    test_task_runner->RunUntilIdle();
  }

  void SetupClient(bool complete_enrollment_before_sync = true) {
    client_->AddObserver(test_observer_.get());

    SendPendingMojoMessages();

    if (complete_enrollment_before_sync)
      InvokeInitialGetLocalMetadataAndThenSync();
    else
      InvokeInitialSyncAndThenGetLocalMetadata();
  }

  void InvokeInitialGetLocalMetadataAndThenSync() {
    EXPECT_FALSE(client_->is_ready());
    EXPECT_EQ(0u, test_observer_->ready_count());
    EXPECT_EQ(0u, test_observer_->enrollment_finished_count());
    EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

    fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
        test_remote_device_list_[0]);

    // Ensure that no Observer callbacks are called until both the local device
    // metadata and the remote devices are supplied.
    EXPECT_FALSE(client_->is_ready());
    EXPECT_EQ(0u, test_observer_->ready_count());
    EXPECT_EQ(0u, test_observer_->enrollment_finished_count());
    EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

    base::RunLoop run_loop;

    fake_device_sync_->InvokePendingGetSyncedDevicesCallback(
        test_remote_device_list_);

    test_observer_->set_closure_for_enrollment_finished(
        run_loop.QuitWhenIdleClosure());
    test_observer_->set_closure_for_new_devices_synced(
        run_loop.QuitWhenIdleClosure());

    run_loop.Run();

    EXPECT_TRUE(client_->is_ready());
    EXPECT_EQ(1u, test_observer_->ready_count());
    EXPECT_EQ(test_remote_device_list_[0].public_key,
              client_->GetLocalDeviceMetadata()->public_key());
    EXPECT_EQ(1u, test_observer_->enrollment_finished_count());
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        client_->GetSyncedDevices(), test_remote_device_list_);
    EXPECT_EQ(1u, test_observer_->new_devices_synced_count());
  }

  void InvokeInitialSyncAndThenGetLocalMetadata() {
    EXPECT_FALSE(client_->is_ready());
    EXPECT_EQ(0u, test_observer_->ready_count());
    EXPECT_EQ(0u, test_observer_->enrollment_finished_count());
    EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

    // Since local device metadata has not yet been supplied at this point,
    // |client_| will queue up another call to fetch it. The callback is handled
    // at the end of this method.
    fake_device_sync_->InvokePendingGetSyncedDevicesCallback(
        test_remote_device_list_);

    // Ensure that no Observer callbacks are called until both the local device
    // metadata and the remote devices are supplied.
    EXPECT_FALSE(client_->is_ready());
    EXPECT_EQ(0u, test_observer_->ready_count());
    EXPECT_EQ(0u, test_observer_->enrollment_finished_count());
    EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

    base::RunLoop run_loop;

    fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
        test_remote_device_list_[0]);

    test_observer_->set_closure_for_new_devices_synced(
        run_loop.QuitWhenIdleClosure());
    test_observer_->set_closure_for_enrollment_finished(
        run_loop.QuitWhenIdleClosure());

    run_loop.Run();

    EXPECT_TRUE(client_->is_ready());
    EXPECT_EQ(1u, test_observer_->ready_count());
    EXPECT_EQ(test_remote_device_list_[0].public_key,
              client_->GetLocalDeviceMetadata()->public_key());
    EXPECT_EQ(1u, test_observer_->enrollment_finished_count());
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        client_->GetSyncedDevices(), test_remote_device_list_);
    EXPECT_EQ(1u, test_observer_->new_devices_synced_count());

    base::RunLoop second_enrollment_run_loop;

    fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
        test_remote_device_list_[0]);
    test_observer_->set_closure_for_enrollment_finished(
        second_enrollment_run_loop.QuitWhenIdleClosure());

    second_enrollment_run_loop.Run();

    // Ensure that the rest of the synced devices are not removed from the cache
    // when updating the local device metadata.
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        client_->GetSyncedDevices(), test_remote_device_list_);
    EXPECT_EQ(2u, test_observer_->enrollment_finished_count());
  }

  void TearDown() override {
    DeviceSyncImpl::Factory::SetCustomFactory(nullptr);
    client_->RemoveObserver(test_observer_.get());
  }

  void CallForceEnrollmentNow(bool expected_success) {
    fake_device_sync_->set_force_enrollment_now_completed_success(
        expected_success);

    base::RunLoop run_loop;
    client_->ForceEnrollmentNow(
        base::BindOnce(&DeviceSyncClientImplTest::OnForceEnrollmentNowCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(expected_success, *force_enrollment_now_completed_success_);
  }

  void CallSyncNow(bool expected_success) {
    fake_device_sync_->set_force_sync_now_completed_success(expected_success);

    base::RunLoop run_loop;
    client_->ForceSyncNow(
        base::BindOnce(&DeviceSyncClientImplTest::OnForceSyncNowCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(expected_success, *force_sync_now_completed_success_);
  }

  void CallSetSoftwareFeatureState(
      mojom::NetworkRequestResult expected_result_code) {
    base::RunLoop run_loop;

    client_->SetSoftwareFeatureState(
        test_remote_device_ref_list_[0].public_key(),
        multidevice::SoftwareFeature::kBetterTogetherHost, true /* enabled */,
        true /* enabled */,
        base::BindOnce(
            &DeviceSyncClientImplTest::OnSetSoftwareFeatureStateCompleted,
            base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingSetSoftwareFeatureStateCallback(
        expected_result_code);
    run_loop.Run();

    EXPECT_EQ(expected_result_code, set_software_feature_state_result_code_);
  }

  void CallSetFeatureStatus(
      mojom::NetworkRequestResult expected_result_code,
      const std::optional<std::string> invalid_instance_id = std::nullopt) {
    base::RunLoop run_loop;

    std::string instance_id = invalid_instance_id.value_or(
        test_remote_device_ref_list_[0].instance_id());

    client_->SetFeatureStatus(
        instance_id, multidevice::SoftwareFeature::kBetterTogetherHost,
        FeatureStatusChange::kEnableExclusively,
        base::BindOnce(&DeviceSyncClientImplTest::OnSetFeatureStatusCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));

    if (!invalid_instance_id) {
      SendPendingMojoMessages();
      fake_device_sync_->InvokePendingSetFeatureStatusCallback(
          expected_result_code);
    }

    run_loop.Run();

    EXPECT_EQ(expected_result_code, set_feature_status_result_code_);
  }

  void CallFindEligibleDevices(
      mojom::NetworkRequestResult expected_result_code,
      multidevice::RemoteDeviceList expected_eligible_devices,
      multidevice::RemoteDeviceList expected_ineligible_devices) {
    base::RunLoop run_loop;

    client_->FindEligibleDevices(
        multidevice::SoftwareFeature::kBetterTogetherHost,
        base::BindOnce(
            &DeviceSyncClientImplTest::OnFindEligibleDevicesCompleted,
            base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingFindEligibleDevicesCallback(
        expected_result_code,
        mojom::FindEligibleDevicesResponse::New(expected_eligible_devices,
                                                expected_ineligible_devices));
    run_loop.Run();

    EXPECT_EQ(expected_result_code,
              std::get<0>(find_eligible_devices_error_code_and_response_));
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        std::get<1>(find_eligible_devices_error_code_and_response_),
        expected_eligible_devices);
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        std::get<2>(find_eligible_devices_error_code_and_response_),
        expected_ineligible_devices);
  }

  void CallNotifyDevices(mojom::NetworkRequestResult expected_result_code,
                         const std::optional<std::vector<std::string>>&
                             invalid_instance_ids = std::nullopt) {
    base::RunLoop run_loop;

    std::vector<std::string> instance_ids =
        invalid_instance_ids.value_or(std::vector<std::string>(
            {test_remote_device_ref_list_[0].instance_id(),
             test_remote_device_ref_list_[1].instance_id()}));

    client_->NotifyDevices(
        instance_ids, cryptauthv2::TargetService::DEVICE_SYNC,
        multidevice::SoftwareFeature::kBetterTogetherHost,
        base::BindOnce(&DeviceSyncClientImplTest::OnNotifyDevicesCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));

    if (!invalid_instance_ids) {
      SendPendingMojoMessages();
      fake_device_sync_->InvokePendingNotifyDevicesCallback(
          expected_result_code);
    }

    run_loop.Run();

    EXPECT_EQ(expected_result_code, notify_devices_result_code_);
  }

  void CallGetDevicesActivityStatus(
      mojom::NetworkRequestResult expected_result_code,
      std::optional<std::vector<mojom::DeviceActivityStatusPtr>>
          expected_activity_statuses) {
    base::RunLoop run_loop;

    client_->GetDevicesActivityStatus(
        base::BindOnce(&DeviceSyncClientImplTest::OnGetDevicesActivityStatus,
                       base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    std::optional<std::vector<mojom::DeviceActivityStatusPtr>>
        device_activity_statuses_optional;
    if (expected_activity_statuses != std::nullopt) {
      std::vector<mojom::DeviceActivityStatusPtr> device_activity_statuses;
      for (const mojom::DeviceActivityStatusPtr& device_activity_status :
           *expected_activity_statuses) {
        device_activity_statuses.emplace_back(mojom::DeviceActivityStatus::New(
            device_activity_status->device_id,
            device_activity_status->last_activity_time,
            device_activity_status->connectivity_status,
            device_activity_status->last_update_time));
      }
      device_activity_statuses_optional =
          std::make_optional(std::move(device_activity_statuses));
    }
    fake_device_sync_->InvokePendingGetDevicesActivityStatusCallback(
        expected_result_code, std::move(device_activity_statuses_optional));
    run_loop.Run();

    EXPECT_EQ(expected_result_code,
              std::get<0>(get_devices_activity_status_code_and_response_));
    EXPECT_EQ(expected_activity_statuses,
              std::get<1>(get_devices_activity_status_code_and_response_));
  }

  void CallGetGroupPrivateKeyStatus(GroupPrivateKeyStatus expected_status) {
    base::RunLoop run_loop;
    client_->GetGroupPrivateKeyStatus(
        base::BindOnce(&DeviceSyncClientImplTest::OnGetGroupPrivateKeyStatus,
                       base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingGetGroupPrivateKeyStatusCallback(
        expected_status);
    run_loop.Run();
    EXPECT_EQ(expected_status, get_group_private_key_status_response_);
  }

  void CallGetBetterTogetherMetadataStatus(
      BetterTogetherMetadataStatus expected_status) {
    base::RunLoop run_loop;
    client_->GetBetterTogetherMetadataStatus(base::BindOnce(
        &DeviceSyncClientImplTest::OnGetBetterTogetherMetadataStatus,
        base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingGetBetterTogetherMetadataStatusCallback(
        expected_status);
    run_loop.Run();
    EXPECT_EQ(expected_status, get_better_together_metadata_status_response_);
  }

  void CallGetDebugInfo() {
    EXPECT_FALSE(debug_info_received_);

    base::RunLoop run_loop;

    client_->GetDebugInfo(
        base::BindOnce(&DeviceSyncClientImplTest::OnGetDebugInfoCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingGetDebugInfoCallback(
        mojom::DebugInfo::New());
    run_loop.Run();

    EXPECT_TRUE(debug_info_received_);
  }

  // DeviceSyncClientImpl cached its devices in a RemoteDeviceCache, which
  // stores devices in an unordered_map -- retrieved devices thus need to be
  // sorted before comparison.
  void VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      multidevice::RemoteDeviceRefList remote_device_ref_list,
      multidevice::RemoteDeviceList remote_device_list) {
    std::vector<std::string> ref_public_keys;
    for (auto device : remote_device_ref_list)
      ref_public_keys.push_back(device.public_key());
    std::sort(ref_public_keys.begin(), ref_public_keys.end(),
              [](auto public_key_1, auto public_key_2) {
                return public_key_1 < public_key_2;
              });

    std::vector<std::string> public_keys;
    for (auto device : remote_device_list)
      public_keys.push_back(device.public_key);
    std::sort(public_keys.begin(), public_keys.end(),
              [](auto public_key_1, auto public_key_2) {
                return public_key_1 < public_key_2;
              });

    EXPECT_EQ(ref_public_keys, public_keys);
  }

  void SendPendingMojoMessages() { client_->FlushForTesting(); }

  const base::test::TaskEnvironment task_environment_;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<gcm::FakeGCMDriver> fake_gcm_driver_;
  testing::NiceMock<MockInstanceIDDriver> fake_instance_id_driver_;
  std::unique_ptr<FakeGcmDeviceInfoProvider> fake_gcm_device_info_provider_;
  std::unique_ptr<FakeClientAppMetadataProvider>
      fake_client_app_metadata_provider_;
  raw_ptr<FakeDeviceSync, DanglingUntriaged> fake_device_sync_;
  std::unique_ptr<FakeDeviceSyncImplFactory> fake_device_sync_impl_factory_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<DeviceSyncBase> device_sync_;
  std::unique_ptr<TestDeviceSyncClientObserver> test_observer_;

  std::unique_ptr<DeviceSyncClientImpl> client_;

  multidevice::RemoteDeviceList test_remote_device_list_;
  const multidevice::RemoteDeviceRefList test_remote_device_ref_list_;

  std::optional<bool> force_enrollment_now_completed_success_;
  std::optional<bool> force_sync_now_completed_success_;
  std::optional<mojom::NetworkRequestResult>
      set_software_feature_state_result_code_;
  std::optional<mojom::NetworkRequestResult> set_feature_status_result_code_;
  std::tuple<mojom::NetworkRequestResult,
             multidevice::RemoteDeviceRefList,
             multidevice::RemoteDeviceRefList>
      find_eligible_devices_error_code_and_response_;
  std::optional<mojom::NetworkRequestResult> notify_devices_result_code_;
  std::tuple<mojom::NetworkRequestResult,
             std::optional<std::vector<mojom::DeviceActivityStatusPtr>>>
      get_devices_activity_status_code_and_response_;
  std::optional<GroupPrivateKeyStatus> get_group_private_key_status_response_;
  std::optional<BetterTogetherMetadataStatus>
      get_better_together_metadata_status_response_;
  bool debug_info_received_ = false;

 private:
  void OnForceEnrollmentNowCompleted(base::OnceClosure quit_closure,
                                     bool success) {
    force_enrollment_now_completed_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnForceSyncNowCompleted(base::OnceClosure quit_closure, bool success) {
    force_sync_now_completed_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnSetSoftwareFeatureStateCompleted(
      base::OnceClosure callback,
      mojom::NetworkRequestResult result_code) {
    set_software_feature_state_result_code_ = result_code;
    std::move(callback).Run();
  }

  void OnSetFeatureStatusCompleted(base::OnceClosure callback,
                                   mojom::NetworkRequestResult result_code) {
    set_feature_status_result_code_ = result_code;
    std::move(callback).Run();
  }

  void OnFindEligibleDevicesCompleted(
      base::OnceClosure callback,
      mojom::NetworkRequestResult result_code,
      multidevice::RemoteDeviceRefList eligible_devices,
      multidevice::RemoteDeviceRefList ineligible_devices) {
    find_eligible_devices_error_code_and_response_ =
        std::make_tuple(result_code, eligible_devices, ineligible_devices);
    std::move(callback).Run();
  }

  void OnNotifyDevicesCompleted(base::OnceClosure callback,
                                mojom::NetworkRequestResult result_code) {
    notify_devices_result_code_ = result_code;
    std::move(callback).Run();
  }

  void OnGetDevicesActivityStatus(
      base::OnceClosure callback,
      mojom::NetworkRequestResult result_code,
      std::optional<std::vector<mojom::DeviceActivityStatusPtr>>
          device_activity_status) {
    get_devices_activity_status_code_and_response_ =
        std::make_tuple(result_code, std::move(device_activity_status));
    std::move(callback).Run();
  }

  void OnGetGroupPrivateKeyStatus(base::OnceClosure callback,
                                  GroupPrivateKeyStatus status) {
    get_group_private_key_status_response_ = status;
    std::move(callback).Run();
  }

  void OnGetBetterTogetherMetadataStatus(base::OnceClosure callback,
                                         BetterTogetherMetadataStatus status) {
    get_better_together_metadata_status_response_ = status;
    std::move(callback).Run();
  }

  void OnGetDebugInfoCompleted(base::OnceClosure callback,
                               mojom::DebugInfoPtr debug_info_ptr) {
    debug_info_received_ = true;
    std::move(callback).Run();
  }
};

TEST_F(DeviceSyncClientImplTest,
       TestCompleteInitialSyncBeforeInitialEnrollment) {
  SetupClient(false /* complete_enrollment_before_sync */);
}

TEST_F(
    DeviceSyncClientImplTest,
    TestCompleteInitialEnrollmentBeforeInitialSync_WaitForLocalDeviceMetadata) {
  client_->AddObserver(test_observer_.get());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(client_->is_ready());
  EXPECT_EQ(0u, test_observer_->ready_count());
  EXPECT_EQ(0u, test_observer_->enrollment_finished_count());
  EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

  // Simulate local device metadata not being ready. It will be ready once
  // synced devices are returned, at which point |client_| should call
  // GetLocalMetadata() again.
  fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(std::nullopt);
  fake_device_sync_->InvokePendingGetSyncedDevicesCallback(
      test_remote_device_list_);

  EXPECT_FALSE(client_->is_ready());
  EXPECT_EQ(0u, test_observer_->ready_count());
  EXPECT_EQ(0u, test_observer_->enrollment_finished_count());
  EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;

  // Simulate the local device metadata now being ready.
  fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
      test_remote_device_list_[0]);

  test_observer_->set_closure_for_enrollment_finished(
      run_loop.QuitWhenIdleClosure());
  test_observer_->set_closure_for_new_devices_synced(
      run_loop.QuitWhenIdleClosure());

  run_loop.Run();

  EXPECT_TRUE(client_->is_ready());
  EXPECT_EQ(1u, test_observer_->ready_count());
  EXPECT_EQ(test_remote_device_list_[0].public_key,
            client_->GetLocalDeviceMetadata()->public_key());
  EXPECT_EQ(1u, test_observer_->enrollment_finished_count());
  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), test_remote_device_list_);
  EXPECT_EQ(1u, test_observer_->new_devices_synced_count());
}

TEST_F(DeviceSyncClientImplTest, TestOnEnrollmentFinished) {
  EXPECT_EQ(0u, test_observer_->enrollment_finished_count());

  SetupClient();

  EXPECT_EQ(test_remote_device_list_[0].public_key,
            client_->GetLocalDeviceMetadata()->public_key());
  EXPECT_EQ(test_remote_device_list_[0].name,
            client_->GetLocalDeviceMetadata()->name());

  fake_device_sync_->NotifyOnEnrollmentFinished();

  // The client calls and waits for DeviceSync::GetLocalDeviceMetadata() to
  // finish before notifying observers that enrollment has finished.
  EXPECT_EQ(1u, test_observer_->enrollment_finished_count());

  base::RunLoop().RunUntilIdle();

  // Update the local device metadata. The last update time must also be later
  // than the previous version, otherwise the update will be ignored by the
  // remote device cache.
  test_remote_device_list_[0].name = "new name";
  test_remote_device_list_[0].last_update_time_millis++;

  base::RunLoop run_loop;
  test_observer_->set_closure_for_enrollment_finished(run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
      test_remote_device_list_[0]);
  run_loop.Run();

  EXPECT_EQ(2u, test_observer_->enrollment_finished_count());

  EXPECT_EQ(test_remote_device_list_[0].public_key,
            client_->GetLocalDeviceMetadata()->public_key());
  EXPECT_EQ("new name", client_->GetLocalDeviceMetadata()->name());
}

TEST_F(DeviceSyncClientImplTest, TestOnNewDevicesSynced) {
  EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

  SetupClient();

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), test_remote_device_list_);

  fake_device_sync_->NotifyOnNewDevicesSynced();

  // The client calls and waits for DeviceSync::GetLocalDeviceMetadata() to
  // finish before notifying observers that enrollment has finished.
  EXPECT_EQ(1u, test_observer_->new_devices_synced_count());

  base::RunLoop().RunUntilIdle();

  // Change the synced device list.
  multidevice::RemoteDeviceList new_device_list(
      {test_remote_device_list_[0], test_remote_device_list_[1]});

  base::RunLoop run_loop;
  test_observer_->set_closure_for_new_devices_synced(run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetSyncedDevicesCallback(new_device_list);
  run_loop.Run();

  EXPECT_EQ(2u, test_observer_->new_devices_synced_count());

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), new_device_list);
}

TEST_F(DeviceSyncClientImplTest, TestForceEnrollmentNow_ExpectSuccess) {
  SetupClient();

  CallForceEnrollmentNow(true /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestForceEnrollmentNow_ExpectFailure) {
  SetupClient();

  CallForceEnrollmentNow(false /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestSyncNow_ExpectSuccess) {
  SetupClient();

  CallSyncNow(true /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestSyncNow_ExpectFailure) {
  SetupClient();

  CallSyncNow(false /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestGetSyncedDevices_DeviceRemovedFromCache) {
  SetupClient();

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), test_remote_device_list_);

  // Remove a device from the test list, and inform |client_|.
  multidevice::RemoteDeviceList new_list(
      {test_remote_device_list_[0], test_remote_device_list_[1],
       test_remote_device_list_[2], test_remote_device_list_[3]});
  client_->OnNewDevicesSynced();

  SendPendingMojoMessages();

  base::RunLoop run_loop;
  test_observer_->set_closure_for_new_devices_synced(run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetSyncedDevicesCallback(new_list);
  run_loop.Run();

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), new_list);
}

TEST_F(DeviceSyncClientImplTest, TestSetSoftwareFeatureState) {
  SetupClient();

  CallSetSoftwareFeatureState(mojom::NetworkRequestResult::kSuccess);
}

TEST_F(DeviceSyncClientImplTest, TestSetFeatureStatus_Success) {
  SetupClient();

  CallSetFeatureStatus(mojom::NetworkRequestResult::kSuccess);
}

TEST_F(DeviceSyncClientImplTest, TestSetFeatureStatus_InvalidInstanceId) {
  SetupClient();

  // Instance ID cannot be empty.
  CallSetFeatureStatus(mojom::NetworkRequestResult::kBadRequest,
                       std::string() /* invalid_instance_id */);

  // Instance ID must be base64 encoded.
  CallSetFeatureStatus(mojom::NetworkRequestResult::kBadRequest,
                       "$%^&*#" /* invalid_instance_id */);

  // Instance ID must be 8 bytes after base64 decoding.
  CallSetFeatureStatus(
      mojom::NetworkRequestResult::kBadRequest,
      "thisislongerthaneightbytes==" /* invalid_instance_id */);
}

TEST_F(DeviceSyncClientImplTest, TestFindEligibleDevices_NoErrorCode) {
  SetupClient();

  multidevice::RemoteDeviceList expected_eligible_devices(
      {test_remote_device_list_[0], test_remote_device_list_[1]});
  multidevice::RemoteDeviceList expected_ineligible_devices(
      {test_remote_device_list_[2], test_remote_device_list_[3],
       test_remote_device_list_[4]});

  CallFindEligibleDevices(mojom::NetworkRequestResult::kSuccess,
                          expected_eligible_devices,
                          expected_ineligible_devices);
}

TEST_F(DeviceSyncClientImplTest, TestFindEligibleDevices_ErrorCode) {
  SetupClient();

  CallFindEligibleDevices(mojom::NetworkRequestResult::kEndpointNotFound,
                          multidevice::RemoteDeviceList(),
                          multidevice::RemoteDeviceList());
}

TEST_F(DeviceSyncClientImplTest, TestNotifyDevices_Success) {
  SetupClient();

  CallNotifyDevices(mojom::NetworkRequestResult::kSuccess);
}

TEST_F(DeviceSyncClientImplTest, TestNotifyDevices_InvalidInstanceIds) {
  SetupClient();

  // Instance IDs cannot be empty, must be base64 encoded, and must be 8 bytes
  // after base64 decoding.
  CallNotifyDevices(
      mojom::NetworkRequestResult::kBadRequest,
      std::vector<std::string>{
          std::string(), "$%^&*#",
          "thisislongerthaneightbytes=="} /* invalid_instance_ids */);
}

TEST_F(DeviceSyncClientImplTest, TestGetDevicesActivityStatus_NoErrorCode) {
  SetupClient();
  std::vector<mojom::DeviceActivityStatusPtr> expected_activity_statuses;
  expected_activity_statuses.emplace_back(mojom::DeviceActivityStatus::New(
      "deviceid", base::Time(), cryptauthv2::ConnectivityStatus::ONLINE,
      base::Time()));

  CallGetDevicesActivityStatus(mojom::NetworkRequestResult::kSuccess,
                               std::move(expected_activity_statuses));
}

TEST_F(DeviceSyncClientImplTest, TestGetDevicesActivityStatus_ErrorCode) {
  SetupClient();

  CallGetDevicesActivityStatus(mojom::NetworkRequestResult::kEndpointNotFound,
                               std::nullopt);
}

TEST_F(DeviceSyncClientImplTest, TestGetDebugInfo) {
  SetupClient();

  CallGetDebugInfo();
}

TEST_F(DeviceSyncClientImplTest, TestGetGroupPrivateKeyStatus) {
  SetupClient();

  CallGetGroupPrivateKeyStatus(
      GroupPrivateKeyStatus::kWaitingForGroupPrivateKey);
  CallGetGroupPrivateKeyStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted);
}

TEST_F(DeviceSyncClientImplTest, TestGetBetterTogetherMetadataStatus) {
  SetupClient();

  CallGetBetterTogetherMetadataStatus(
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);
  CallGetBetterTogetherMetadataStatus(
      BetterTogetherMetadataStatus::kMetadataDecrypted);
}

}  // namespace device_sync

}  // namespace ash
