// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler_factory.h"
#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"
#include "chromeos/ash/components/nearby/presence/credentials/fake_local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/fake_nearby_presence_server_client.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kDeviceName = "Test's Chromebook";
const std::string kUserName = "Test Tester";
const std::string kProfileUrl = "https://example.com";
const std::string kDeviceId = "0123456789";
const base::TimeDelta kServerResponseTimeout = base::Seconds(5);
constexpr int kMaxUpdateCredentialRequestCount = 6;
std::vector<base::TimeDelta> kUpdateCredentialCoolDownPeriods = {
    base::Seconds(0), base::Seconds(15), base::Seconds(30), base::Minutes(1),
    base::Minutes(2), base::Minutes(5),  base::Minutes(10)};
constexpr int kServerCommunicationMaxAttempts = 5;
const std::vector<uint8_t> kBluetoothMacAddress = {0x12, 0x34, 0x56,
                                                   0x78, 0x9a, 0xbc};
const std::vector<uint8_t> kMetadataDeviceId = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
const long kId1 = 111;
const long kId2 = 222;
const long kId3 = 333;
const std::vector<uint8_t> kKeySeed = {
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44};

ash::nearby::presence::mojom::SharedCredentialPtr BuildSharedCredential(
    long id) {
  ash::nearby::presence::mojom::SharedCredentialPtr cred =
      ash::nearby::presence::mojom::SharedCredential::New();
  cred->id = id;
  // To communicate across the wire this field on the mojo struct needs to be
  // set since the mojo wire checks for this array to be size 32.
  cred->key_seed = kKeySeed;
  return cred;
}

std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
BuildSharedCredentials() {
  std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
      shared_credentials;
  shared_credentials.push_back(BuildSharedCredential(kId1));
  shared_credentials.push_back(BuildSharedCredential(kId2));
  shared_credentials.push_back(BuildSharedCredential(kId3));
  return shared_credentials;
}

::nearby::internal::DeviceIdentityMetaData BuildTestMetadata() {
  return ash::nearby::presence::proto::BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS,
      /*device_name=*/kDeviceName,
      /*bluetooth_mac_address=*/
      std::string(kBluetoothMacAddress.begin(), kBluetoothMacAddress.end()),
      /*device_id=*/
      std::string(kMetadataDeviceId.begin(), kMetadataDeviceId.end()));
}

}  // namespace

namespace ash::nearby::presence {

class TestCreator final : public NearbyPresenceCredentialManagerImpl::Creator {
 public:
  ~TestCreator() override = default;

  void Create(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
      std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider,
      CreateCallback on_created) override {
    NearbyPresenceCredentialManagerImpl::Creator::Create(
        pref_service, identity_manager, url_loader_factory, nearby_presence,
        std::move(local_device_data_provider), std::move(on_created));
  }

  void ResetHasCredentialManagerBeenCreated() {
    has_credential_manager_been_created_ = false;
  }
};

class NearbyPresenceCredentialManagerImplTest : public testing::Test {
 protected:
  NearbyPresenceCredentialManagerImplTest() {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  void SetUp() override {
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(
        &scheduler_factory_);
    NearbyPresenceServerClientImpl::Factory::SetFactoryForTesting(
        &server_client_factory_);
    local_device_data_provider_ =
        std::make_unique<FakeLocalDeviceDataProvider>();
    fake_local_device_data_provider_ =
        static_cast<FakeLocalDeviceDataProvider*>(
            local_device_data_provider_.get());

    // Simulate first time registration flow.
    fake_local_device_data_provider_->SetRegistrationComplete(false);

    // Even before registration, the LocalDeviceDataProvider can provide
    // all fields in its Metadata. Simulate that.
    fake_local_device_data_provider_->SetDeviceMetadata(BuildTestMetadata());

    // Simulate the device id which will be generated in a call to
    // |GetDeviceId|.
    fake_local_device_data_provider_->SetDeviceId(kDeviceId);

    // Simulate the credentials being generated in the NP library.
    fake_nearby_presence_.SetGenerateCredentialsResponse(
        BuildSharedCredentials(), mojo_base::mojom::AbslStatusCode::kOk);
  }

  void TearDown() override {
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyPresenceServerClientImpl::Factory::SetFactoryForTesting(nullptr);
    credential_manager_creator_.ResetHasCredentialManagerBeenCreated();
    fake_local_device_data_provider_ = nullptr;
  }

  void TriggerFirstTimeRegistrationSuccess() {
    // Simulate the scheduler notifying the CredentialManager that the task is
    // ready when it has network connectivity.
    const raw_ptr<FakeNearbyScheduler, DanglingUntriaged>
        first_time_registration_scheduler =
            scheduler_factory_.pref_name_to_on_demand_instance()
                .find(
                    prefs::
                        kNearbyPresenceSchedulingFirstTimeRegistrationPrefName)
                ->second.fake_scheduler;
    first_time_registration_scheduler->InvokeRequestCallback();

    // Mock and return the server response.
    ash::nearby::proto::UpdateDeviceResponse response;
    response.set_person_name(kUserName);
    response.set_image_url(kProfileUrl);
    server_client_factory_.fake_server_client()
        ->InvokeUpdateDeviceSuccessCallback(response);
  }

  void TriggerLocalCredentialUploadSuccess() {
    // Simulate the scheduler notifying the CredentialManager that the upload
    // task is ready when it has network connectivity.
    const raw_ptr<FakeNearbyScheduler, DanglingUntriaged> upload_scheduler =
        scheduler_factory_.pref_name_to_on_demand_instance()
            .find(prefs::kNearbyPresenceSchedulingUploadPrefName)
            ->second.fake_scheduler;
    upload_scheduler->InvokeRequestCallback();

    // Mock and return the server response.
    ash::nearby::proto::UpdateDeviceResponse update_credentials_response;
    server_client_factory_.fake_server_client()
        ->InvokeUpdateDeviceSuccessCallback(update_credentials_response);
  }

  void TriggerDownloadRemoteCredentialSuccess() {
    // Simulate the scheduler notifying the CredentialManager that the download
    // task is ready when it has network connectivity.
    const raw_ptr<FakeNearbyScheduler, DanglingUntriaged> download_scheduler =
        scheduler_factory_.pref_name_to_on_demand_instance()
            .find(prefs::kNearbyPresenceSchedulingDownloadPrefName)
            ->second.fake_scheduler;
    download_scheduler->InvokeRequestCallback();

    // Next, mock and return the server response for fetching remote device
    // public certificates.
    ash::nearby::proto::ListSharedCredentialsResponse certificate_response;
    server_client_factory_.fake_server_client()
        ->InvokeListSharedCredentialsSuccessCallback({certificate_response});
  }

  void CreateCredentialManager(base::OnceClosure on_created) {
    credential_manager_creator_.Create(
        &pref_service_, identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        fake_nearby_presence_.shared_remote(),
        std::move(local_device_data_provider_),
        base::BindLambdaForTesting(
            [&, callback = base::BindOnce(std::move(on_created))](
                std::unique_ptr<NearbyPresenceCredentialManager>
                    credential_manager) {
              credential_manager_ = std::move(credential_manager);

              // OnceCallback::Run() may only be invoked on a non-const rvalue,
              // so we convert it here to allow the execution.
              std::move(const_cast<base::OnceClosure&>(callback)).Run();
            }));
  }

  void SimulateDeviceAlreadyRegistered() {
    // Simulate that it is not first time registration flow.
    fake_local_device_data_provider_->SetRegistrationComplete(true);
    fake_local_device_data_provider_->SaveUserRegistrationInfo(
        /*display_name=*/kUserName, /*image_url=*/kProfileUrl);
  }

  void SetUpDailySync(bool have_credentials_changed,
                      bool get_local_shared_credentials_success) {
    SimulateDeviceAlreadyRegistered();

    // Simulate that the local credentials have not changed, which is expected
    // to not trigger a local credential upload to the server.
    fake_local_device_data_provider_->SetHaveSharedCredentialsChanged(
        have_credentials_changed);

    if (get_local_shared_credentials_success) {
      // Simulate the local device credentials stored in the NP library and
      // retrieved successfully.
      fake_nearby_presence_.SetLocalSharedCredentialsResponse(
          BuildSharedCredentials(), mojo_base::mojom::AbslStatusCode::kOk);
    } else {
      // Simulate the local device credentials retrieved unsuccessfully.
      fake_nearby_presence_.SetLocalSharedCredentialsResponse(
          /*credentials=*/{}, mojo_base::mojom::AbslStatusCode::kUnknown);
    }

    // Simulate the remote device credentials being successfully set in the
    // NP library.
    fake_nearby_presence_.SetUpdateRemoteCredentialsStatus(
        mojo_base::mojom::AbslStatusCode::kOk);

    base::RunLoop update_local_device_metadata_run_loop;
    fake_nearby_presence_.SetUpdateLocalDeviceMetadataCallback(
        update_local_device_metadata_run_loop.QuitClosure());

    base::RunLoop create_credential_run_loop;
    CreateCredentialManager(create_credential_run_loop.QuitClosure());
    create_credential_run_loop.Run();

    EXPECT_TRUE(credential_manager_);

    update_local_device_metadata_run_loop.Run();

    daily_sync_scheduler_ =
        scheduler_factory_.pref_name_to_periodic_instance()
            .find(prefs::kNearbyPresenceSchedulingCredentialDailySyncPrefName)
            ->second.fake_scheduler;
  }

  void TriggerDailySync(bool have_credentials_changed,
                        bool get_local_shared_credentials_success) {
    SetUpDailySync(have_credentials_changed,
                   get_local_shared_credentials_success);

    // Simulate the scheduler notifying the CredentialManager that the task is
    // ready when it has network connectivity.
    daily_sync_scheduler_->InvokeRequestCallback();
  }

  void UpdateCredentialsDailySync() {
    base::RunLoop get_local_creds_run_loop;
    fake_local_device_data_provider_->SetHaveSharedCredentialsChangedCallback(
        get_local_creds_run_loop.QuitClosure());

    credential_manager_->UpdateCredentials();
    daily_sync_scheduler_->InvokeRequestCallback();

    // A second call to `UpdateCredentials()` is ignored since the first one is
    // in progress.
    credential_manager_->UpdateCredentials();

    // Required to send messages across mojo pipe for saving remote device
    // credentials.
    base::RunLoop save_remote_creds_run_loop;
    daily_sync_scheduler_->SetHandleResultCallback(
        save_remote_creds_run_loop.QuitClosure());

    get_local_creds_run_loop.Run();

    // Expect no calls to trigger a credential upload, which is indicated by the
    // creation of an on demand upload scheduler.
    EXPECT_EQ(scheduler_factory_.pref_name_to_on_demand_instance().find(
                  prefs::kNearbyPresenceSchedulingUploadPrefName),
              scheduler_factory_.pref_name_to_on_demand_instance().end());

    // Simulate a successful download of credentials from the server.
    TriggerDownloadRemoteCredentialSuccess();

    save_remote_creds_run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeNearbyPresence fake_nearby_presence_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<FakeLocalDeviceDataProvider> fake_local_device_data_provider_ =
      nullptr;
  raw_ptr<FakeNearbyScheduler, DanglingUntriaged> daily_sync_scheduler_ =
      nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;
  std::unique_ptr<NearbyPresenceCredentialManager> credential_manager_;
  TestCreator credential_manager_creator_;
  FakeNearbyPresenceServerClient::Factory server_client_factory_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NearbyPresenceCredentialManagerImplTest, SetDeviceMetadata) {
  SimulateDeviceAlreadyRegistered();

  base::RunLoop update_local_device_metadata_run_loop;
  fake_nearby_presence_.SetUpdateLocalDeviceMetadataCallback(
      update_local_device_metadata_run_loop.QuitClosure());

  base::RunLoop create_credential_run_loop;
  CreateCredentialManager(create_credential_run_loop.QuitClosure());
  create_credential_run_loop.Run();

  EXPECT_TRUE(credential_manager_);

  update_local_device_metadata_run_loop.Run();

  auto* local_device_metadata = fake_nearby_presence_.GetLocalDeviceMetadata();
  EXPECT_TRUE(local_device_metadata);
  EXPECT_EQ(mojom::PresenceDeviceType::kChromeos,
            local_device_metadata->device_type);
  EXPECT_EQ(kDeviceName, local_device_metadata->device_name);
  EXPECT_EQ(kBluetoothMacAddress, local_device_metadata->bluetooth_mac_address);
  EXPECT_EQ(kMetadataDeviceId, local_device_metadata->device_id);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, RegistrationSuccess) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop generate_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      generate_creds_run_loop.QuitClosure());

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  // Any requests to daily sync are expected to be ignored. If they are not
  // ignored, a crash occurs due to the `server_cliet`_ already existing. Test
  // this by ensuring no crash.
  daily_sync_scheduler_ =
      scheduler_factory_.pref_name_to_periodic_instance()
          .find(prefs::kNearbyPresenceSchedulingCredentialDailySyncPrefName)
          ->second.fake_scheduler;
  daily_sync_scheduler_->InvokeRequestCallback();

  generate_creds_run_loop.Run();

  TriggerLocalCredentialUploadSuccess();
  TriggerDownloadRemoteCredentialSuccess();

  create_credential_manager_run_loop.Run();
  EXPECT_TRUE(credential_manager_);
  EXPECT_TRUE(credential_manager_->IsLocalDeviceRegistered());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.FirstTimeRegistration.Result",
      /*bucket: kSuccess=*/0, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration.FailureReason",
      0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration."
      "AttemptsNeededCount",
      /*bucket: attempt_count=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration."
      "ServerRequestDuration",
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.Result", /*bucket: success=*/true, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.FailureReason", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.AttemptsNeededCount",
      /*bucket: attempt_count=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration", 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.Result", /*bucket: success=*/true,
      1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.FailureReason", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.AttemptsNeededCount",
      /*bucket: attempt_count=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.ServerRequestDuration", 1);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationTimeout) {
  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  // Simulate the max number of failures caused by a server response timeout.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged>
      first_time_registration_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(
                  prefs::kNearbyPresenceSchedulingFirstTimeRegistrationPrefName)
              ->second.fake_scheduler;
  first_time_registration_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  first_time_registration_scheduler->InvokeRequestCallback();
  task_environment_.FastForwardBy(kServerResponseTimeout);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.FirstTimeRegistration.Result",
      /*bucket: kRegistrationWithServerFailure=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration.FailureReason",
      /*bucket: NearbyHttpResult::kTimeout*/
      ash::nearby::NearbyHttpResult::kTimeout, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration."
      "ServerRequestDuration",
      0);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationFailure) {
  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  // Simulate the max number of failures caused by a RPC failure.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged>
      first_time_registration_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(
                  prefs::kNearbyPresenceSchedulingFirstTimeRegistrationPrefName)
              ->second.fake_scheduler;
  first_time_registration_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  first_time_registration_scheduler->InvokeRequestCallback();
  server_client_factory_.fake_server_client()->InvokeUpdateDeviceErrorCallback(
      ash::nearby::NearbyHttpError::kInternalServerError);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.FirstTimeRegistration.Result",
      /*bucket: kRegistrationWithServerFailure=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration.FailureReason",
      /*bucket: NearbyHttpResult::kHttpErrorInternalServerError*/
      ash::nearby::NearbyHttpResult::kHttpErrorInternalServerError, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration."
      "ServerRequestDuration",
      0);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, CredentialGenerationFailure) {
  // Simulate the credentials being failed to be generated in the NP library.
  fake_nearby_presence_.SetGenerateCredentialsResponse(
      {}, mojo_base::mojom::AbslStatusCode::kFailedPrecondition);

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.FirstTimeRegistration.Result",
      /*bucket: kLocalCredentialGenerationFailure=*/2, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.FirstTimeServerRegistration."
      "ServerRequestDuration",
      1);
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       UploadCredentialsServerTimeout) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop generate_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      generate_creds_run_loop.QuitClosure());

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe
  generate_creds_run_loop.Run();

  // Simulate the scheduler notifying the CredentialManager that the task is
  // ready when it has network connectivity.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged>
      first_time_upload_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingUploadPrefName)
              ->second.fake_scheduler;
  first_time_upload_scheduler->InvokeRequestCallback();

  // Simulate the max number of failures caused by a server response timeout.
  first_time_upload_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  task_environment_.FastForwardBy(kServerResponseTimeout);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.Result", /*bucket: success=*/false,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.FailureReason",
      /*bucket: NearbyHttpResult::kTimeout*/
      ash::nearby::NearbyHttpResult::kTimeout, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration", 0);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, UploadCredentialsFailure) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop generate_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      generate_creds_run_loop.QuitClosure());

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  generate_creds_run_loop.Run();

  // Simulate the scheduler notifying the CredentialManager that the upload
  // task is ready when it has network connectivity.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged>
      first_time_upload_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingUploadPrefName)
              ->second.fake_scheduler;

  // Simulate the max number of failures caused by a RPC failure.
  first_time_upload_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  first_time_upload_scheduler->InvokeRequestCallback();
  server_client_factory_.fake_server_client()->InvokeUpdateDeviceErrorCallback(
      ash::nearby::NearbyHttpError::kInternalServerError);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.Result", /*bucket: success=*/false,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.FailureReason",
      /*bucket: NearbyHttpResult::kHttpErrorInternalServerError*/
      ash::nearby::NearbyHttpResult::kHttpErrorInternalServerError, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration", 0);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, DownloadCredentialsFailure) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop generate_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      generate_creds_run_loop.QuitClosure());

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();
  // Required for credentials to be generated and passed over the mojo pipe.
  generate_creds_run_loop.Run();

  TriggerLocalCredentialUploadSuccess();

  // Simulate the scheduler notifying the CredentialManager that the download
  // task is ready when it has network connectivity.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged>
      first_time_download_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingDownloadPrefName)
              ->second.fake_scheduler;

  // Simulate the max number of failures caused by a RPC failure.
  first_time_download_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  first_time_download_scheduler->InvokeRequestCallback();
  ash::nearby::proto::ListSharedCredentialsResponse certificate_response;
  server_client_factory_.fake_server_client()
      ->InvokeListSharedCredentialsErrorCallback(
          ash::nearby::NearbyHttpError::kInternalServerError);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.Result", /*bucket: success=*/false,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.FailureReason",
      /*bucket: NearbyHttpResult::kHttpErrorInternalServerError*/
      ash::nearby::NearbyHttpResult::kHttpErrorInternalServerError, 1);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, DownloadCredentialsTimeout) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop generate_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      generate_creds_run_loop.QuitClosure());

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  generate_creds_run_loop.Run();

  TriggerLocalCredentialUploadSuccess();

  // Simulate the scheduler notifying the CredentialManager that the download
  // task is ready when it has network connectivity.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged>
      first_time_download_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingDownloadPrefName)
              ->second.fake_scheduler;
  first_time_download_scheduler->InvokeRequestCallback();

  // Simulate the max number of failures caused by a server response timeout.
  first_time_download_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  task_environment_.FastForwardBy(kServerResponseTimeout);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.Result", /*bucket: success=*/false,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.FailureReason",
      /*bucket: NearbyHttpResult::kTimeout*/
      ash::nearby::NearbyHttpResult::kTimeout, 1);
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       UpdateRemoteCredentialsFailure) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop generate_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      generate_creds_run_loop.QuitClosure());

  // Simulate the remote device credentials being unsuccessfully set in the
  // NP library.
  fake_nearby_presence_.SetUpdateRemoteCredentialsStatus(
      mojo_base::mojom::AbslStatusCode::kDeadlineExceeded);

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  generate_creds_run_loop.Run();

  TriggerLocalCredentialUploadSuccess();
  TriggerDownloadRemoteCredentialSuccess();

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       DailySyncSuccess_LocalCredentialsChanged) {
  // Required to send messages across mojo pipe for retrieving local device
  // credentials.
  base::RunLoop get_local_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      get_local_creds_run_loop.QuitClosure());

  // Simulate that the local credentials have changed, which is expected to
  // trigger a local credential upload to the server.
  TriggerDailySync(/*have_credentials_changed=*/true,
                   /*get_local_shared_credentials_success=*/true);

  // Required to send messages across mojo pipe for saving remote device
  // credentials.
  base::RunLoop save_remote_creds_run_loop;
  daily_sync_scheduler_->SetHandleResultCallback(
      save_remote_creds_run_loop.QuitClosure());

  get_local_creds_run_loop.Run();

  // Simulate a successful result from the server. This call also enforces
  // that an upload request has been made.
  TriggerLocalCredentialUploadSuccess();

  // Simulate a successful download of credentials from the server.
  TriggerDownloadRemoteCredentialSuccess();

  save_remote_creds_run_loop.Run();

  // Expect daily sync success, which only happens after credentials are
  // saved to the NP library.
  EXPECT_TRUE(daily_sync_scheduler_->handled_results().front());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.Result", /*bucket: success=*/true, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.FailureReason", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.AttemptsNeededCount",
      /*bucket: attempt_count=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration", 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.Result", /*bucket: success=*/true,
      1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.FailureReason", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.AttemptsNeededCount",
      /*bucket: attempt_count=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.ServerRequestDuration", 1);
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       DailySyncSuccess_LocalCredentialsDidntChanged) {
  base::RunLoop get_local_creds_run_loop;
  fake_local_device_data_provider_->SetHaveSharedCredentialsChangedCallback(
      get_local_creds_run_loop.QuitClosure());

  TriggerDailySync(/*have_credentials_changed=*/false,
                   /*get_local_shared_credentials_success=*/true);

  // Required to send messages across mojo pipe for saving remote device
  // credentials.
  base::RunLoop save_remote_creds_run_loop;
  daily_sync_scheduler_->SetHandleResultCallback(
      save_remote_creds_run_loop.QuitClosure());

  get_local_creds_run_loop.Run();

  // Expect no calls to trigger a credential upload, which is indicated by the
  // creation of an on demand upload scheduler.
  EXPECT_EQ(scheduler_factory_.pref_name_to_on_demand_instance().find(
                prefs::kNearbyPresenceSchedulingUploadPrefName),
            scheduler_factory_.pref_name_to_on_demand_instance().end());

  // Simulate a successful download of credentials from the server.
  TriggerDownloadRemoteCredentialSuccess();

  save_remote_creds_run_loop.Run();

  // Expect daily sync success, which only happens after credentials are
  // saved to the NP library.
  EXPECT_TRUE(daily_sync_scheduler_->handled_results().front());
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.Result", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.FailureReason", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.AttemptsNeededCount", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.Result", /*bucket: success=*/true,
      1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.FailureReason", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.AttemptsNeededCount",
      /*bucket: attempt_count=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.ServerRequestDuration", 1);
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       DailySyncFailure_GetLocalCredentialFailure) {
  TriggerDailySync(/*have_credentials_changed=*/false,
                   /*get_local_shared_credentials_success=*/false);

  // Required because no way to inject a QuitClosure() into the
  // FakeLocalDeviceProvider, since it is unused in this flow.
  base::RunLoop().RunUntilIdle();

  // Expect no calls to trigger a credential upload, which is indicated by the
  // creation of an on demand upload scheduler.
  EXPECT_EQ(scheduler_factory_.pref_name_to_on_demand_instance().find(
                prefs::kNearbyPresenceSchedulingUploadPrefName),
            scheduler_factory_.pref_name_to_on_demand_instance().end());

  // Expect daily sync failure, which also indicates exponential retries.
  EXPECT_FALSE(daily_sync_scheduler_->handled_results().front());
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       DailySyncFailure_UploadCredentialsFailure) {
  base::RunLoop get_local_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      get_local_creds_run_loop.QuitClosure());

  TriggerDailySync(/*have_credentials_changed=*/true,
                   /*get_local_shared_credentials_success=*/true);
  get_local_creds_run_loop.Run();

  // Simulate failure to upload credentials to the server.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged> upload_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(prefs::kNearbyPresenceSchedulingUploadPrefName)
          ->second.fake_scheduler;
  upload_scheduler->SetNumConsecutiveFailures(kServerCommunicationMaxAttempts);
  upload_scheduler->InvokeRequestCallback();
  server_client_factory_.fake_server_client()->InvokeUpdateDeviceErrorCallback(
      ash::nearby::NearbyHttpError::kInternalServerError);

  // Expect daily sync failure, which also indicates exponential retries.
  EXPECT_FALSE(daily_sync_scheduler_->handled_results().front());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.Result", /*bucket: success=*/false,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Upload.FailureReason",
      /*bucket: NearbyHttpResult::kHttpErrorInternalServerError*/
      ash::nearby::NearbyHttpResult::kHttpErrorInternalServerError, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.AttemptsNeededCount", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration", 0);
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       DailySyncFailure_DownloadCredentialFailure) {
  base::RunLoop get_local_creds_run_loop;
  fake_local_device_data_provider_->SetHaveSharedCredentialsChangedCallback(
      get_local_creds_run_loop.QuitClosure());

  TriggerDailySync(/*have_credentials_changed=*/false,
                   /*get_local_shared_credentials_success=*/true);

  get_local_creds_run_loop.Run();

  // Simulate failure to download credentials from the server.
  const raw_ptr<FakeNearbyScheduler, DanglingUntriaged> upload_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(prefs::kNearbyPresenceSchedulingDownloadPrefName)
          ->second.fake_scheduler;
  upload_scheduler->SetNumConsecutiveFailures(kServerCommunicationMaxAttempts);
  upload_scheduler->InvokeRequestCallback();
  server_client_factory_.fake_server_client()
      ->InvokeListSharedCredentialsErrorCallback(
          ash::nearby::NearbyHttpError::kInternalServerError);

  EXPECT_FALSE(daily_sync_scheduler_->handled_results().front());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.Result", /*bucket: success=*/false,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.FailureReason",
      /*bucket: NearbyHttpResult::kHttpErrorInternalServerError*/
      ash::nearby::NearbyHttpResult::kHttpErrorInternalServerError, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.AttemptsNeededCount", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.ServerRequestDuration", 0);
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       DailySyncFailure_SaveRemoteCredentialFailure) {
  base::RunLoop get_local_creds_run_loop;
  fake_local_device_data_provider_->SetHaveSharedCredentialsChangedCallback(
      get_local_creds_run_loop.QuitClosure());

  TriggerDailySync(/*have_credentials_changed=*/false,
                   /*get_local_shared_credentials_success=*/true);

  // Required to send messages across mojo pipe for saving remote device
  // credentials.
  base::RunLoop save_remote_creds_run_loop;
  daily_sync_scheduler_->SetHandleResultCallback(
      save_remote_creds_run_loop.QuitClosure());

  get_local_creds_run_loop.Run();

  // Simulate the remote device credentials being unsuccessfully set in the
  // NP library.
  fake_nearby_presence_.SetUpdateRemoteCredentialsStatus(
      mojo_base::mojom::AbslStatusCode::kUnknown);

  // Simulate a successful download of credentials from the server.
  TriggerDownloadRemoteCredentialSuccess();
  save_remote_creds_run_loop.Run();

  EXPECT_FALSE(daily_sync_scheduler_->handled_results().front());
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       DailySyncSuccess_TriggeredByUpdateCredentials) {
  SetUpDailySync(/*have_credentials_changed=*/false,
                 /*get_local_shared_credentials_success=*/true);
  UpdateCredentialsDailySync();
  EXPECT_TRUE(daily_sync_scheduler_->handled_results().front());
  size_t expected_num_requests = 1;

  for (int i = 1; i <= kMaxUpdateCredentialRequestCount; ++i) {
    EXPECT_EQ(expected_num_requests,
              daily_sync_scheduler_->num_immediate_requests());

    // Once the cooloff period has passed, expect the number of requests to
    // increase since the call to `UpdateCredentials()` is not ignored.
    expected_num_requests++;
    task_environment_.FastForwardBy(kUpdateCredentialCoolDownPeriods[i]);
    UpdateCredentialsDailySync();
    EXPECT_EQ(expected_num_requests,
              daily_sync_scheduler_->num_immediate_requests());
    EXPECT_TRUE(daily_sync_scheduler_->handled_results().front());
  }

  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.Result", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.FailureReason", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.AttemptsNeededCount", 0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Upload.ServerRequestDuration", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.Result", /*bucket: success=*/true,
      7);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.FailureReason", 0);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Download.AttemptsNeededCount",
      /*bucket: attempt_count=*/1, 7);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Download.ServerRequestDuration", 7);
}

}  // namespace ash::nearby::presence
