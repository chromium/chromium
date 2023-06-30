// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler_factory.h"
#include "chromeos/ash/components/nearby/presence/credentials/fake_local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/fake_nearby_presence_server_client.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/ash/components/nearby/presence/credentials/proto_conversions.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"
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

const std::string kUserEmail = "testtester@gmail.com";
const std::string kDeviceName = "Test's Chromebook";
const std::string kUserName = "Test Tester";
const std::string kProfileUrl = "https://example.com";
const std::string kDeviceId = "0123456789";
const base::TimeDelta kServerResponseTimeout = base::Seconds(5);
constexpr int kServerCommunicationMaxAttempts = 5;
const std::vector<uint8_t> kSecretId1 = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
const std::vector<uint8_t> kSecretId2 = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};
const std::vector<uint8_t> kSecretId3 = {0x33, 0x33, 0x33, 0x33, 0x33, 0x33};
const std::vector<uint8_t> kKeySeed = {
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44};

ash::nearby::presence::mojom::SharedCredentialPtr BuildSharedCredential(
    std::vector<uint8_t> secret_id) {
  ash::nearby::presence::mojom::SharedCredentialPtr cred =
      ash::nearby::presence::mojom::SharedCredential::New();
  cred->secret_id = secret_id;
  // To communicate across the wire this field on the mojo struct needs to be
  // set since the mojo wire checks for this array to be size 32.
  cred->key_seed = kKeySeed;
  return cred;
}

std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
BuildSharedCredentials() {
  std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
      shared_credentials;
  shared_credentials.push_back(BuildSharedCredential(kSecretId1));
  shared_credentials.push_back(BuildSharedCredential(kSecretId2));
  shared_credentials.push_back(BuildSharedCredential(kSecretId3));
  return shared_credentials;
}

::nearby::internal::Metadata BuildTestMetadata() {
  return ash::nearby::presence::proto::BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS,
      /*account_name=*/kUserEmail,
      /*device_name=*/kDeviceName,
      /*user_name=*/kUserName,
      /*profile_url=*/kProfileUrl,
      /*mac_address=*/std::string());
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

    // Simulate the device id which will be generated in a call to
    // |GetDeviceId|.
    fake_local_device_data_provider_->SetDeviceId(kDeviceId);

    // Simulate the credentials being generated in the NP library.
    fake_nearby_presence_.SetGenerateCredentialsResponse(
        BuildSharedCredentials(), mojom::StatusCode::kOk);
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
    const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
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

  void TriggerFirstTimeLocalCredentialUploadSuccess() {
    // Simulate the scheduler notifying the CredentialManager that the upload
    // task is ready when it has network connectivity.
    const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
        first_time_upload_scheduler =
            scheduler_factory_.pref_name_to_on_demand_instance()
                .find(prefs::kNearbyPresenceSchedulingFirstTimeUploadPrefName)
                ->second.fake_scheduler;
    first_time_upload_scheduler->InvokeRequestCallback();

    // Mock and return the server response.
    ash::nearby::proto::UpdateDeviceResponse update_credentials_response;
    server_client_factory_.fake_server_client()
        ->InvokeUpdateDeviceSuccessCallback(update_credentials_response);
  }

  void TriggerFirstTimeDownloadRemoteCredentialSuccess() {
    // Simulate the scheduler notifying the CredentialManager that the download
    // task is ready when it has network connectivity.
    const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
        first_time_download_scheduler =
            scheduler_factory_.pref_name_to_on_demand_instance()
                .find(prefs::kNearbyPresenceSchedulingFirstTimeDownloadPrefName)
                ->second.fake_scheduler;
    first_time_download_scheduler->InvokeRequestCallback();

    // Next, mock and return the server response for fetching remote device
    // public certificates.
    ash::nearby::proto::ListPublicCertificatesResponse certificate_response;
    server_client_factory_.fake_server_client()
        ->InvokeListPublicCertificatesSuccessCallback({certificate_response});
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

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeNearbyPresence fake_nearby_presence_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<FakeLocalDeviceDataProvider> fake_local_device_data_provider_ =
      nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;
  std::unique_ptr<NearbyPresenceCredentialManager> credential_manager_;
  TestCreator credential_manager_creator_;
  FakeNearbyPresenceServerClient::Factory server_client_factory_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(NearbyPresenceCredentialManagerImplTest, SetDeviceMetadata) {
  // Simulate that it is not first time registration flow.
  fake_local_device_data_provider_->SetRegistrationComplete(true);
  fake_local_device_data_provider_->SetDeviceMetadata(BuildTestMetadata());
  fake_local_device_data_provider_->SaveUserRegistrationInfo(
      /*display_name=*/kUserName, /*image_url=*/kProfileUrl);

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
  EXPECT_EQ(kProfileUrl, local_device_metadata->device_profile_url);
  EXPECT_EQ(kUserName, local_device_metadata->user_name);
  EXPECT_EQ(kUserEmail, local_device_metadata->account_name);
  EXPECT_EQ(mojom::PresenceDeviceType::kChromeos,
            local_device_metadata->device_type);
  EXPECT_EQ(kDeviceName, local_device_metadata->device_name);
}

TEST_F(NearbyPresenceCredentialManagerImplTest, RegistrationSuccess) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop generate_creds_run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      generate_creds_run_loop.QuitClosure());

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  generate_creds_run_loop.Run();

  TriggerFirstTimeLocalCredentialUploadSuccess();
  TriggerFirstTimeDownloadRemoteCredentialSuccess();

  create_credential_manager_run_loop.Run();
  EXPECT_TRUE(credential_manager_);
  EXPECT_TRUE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationTimeout) {
  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  // Simulate the max number of failures caused by a server response timeout.
  const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
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
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationFailure) {
  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  // Simulate the max number of failures caused by a RPC failure.
  const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
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
}

TEST_F(NearbyPresenceCredentialManagerImplTest, CredentialGenerationFailure) {
  // Simulate the credentials being failed to be generated in the NP library.
  fake_nearby_presence_.SetGenerateCredentialsResponse(
      {}, mojom::StatusCode::kFailure);

  base::RunLoop create_credential_manager_run_loop;
  CreateCredentialManager(create_credential_manager_run_loop.QuitClosure());

  TriggerFirstTimeRegistrationSuccess();

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
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
  const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
      first_time_upload_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingFirstTimeUploadPrefName)
              ->second.fake_scheduler;
  first_time_upload_scheduler->InvokeRequestCallback();

  // Simulate the max number of failures caused by a server response timeout.
  first_time_upload_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  task_environment_.FastForwardBy(kServerResponseTimeout);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
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
  const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
      first_time_upload_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingFirstTimeUploadPrefName)
              ->second.fake_scheduler;

  // Simulate the max number of failures caused by a RPC failure.
  first_time_upload_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  first_time_upload_scheduler->InvokeRequestCallback();
  server_client_factory_.fake_server_client()->InvokeUpdateDeviceErrorCallback(
      ash::nearby::NearbyHttpError::kInternalServerError);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
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

  TriggerFirstTimeLocalCredentialUploadSuccess();

  // Simulate the scheduler notifying the CredentialManager that the download
  // task is ready when it has network connectivity.
  const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
      first_time_download_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingFirstTimeDownloadPrefName)
              ->second.fake_scheduler;

  // Simulate the max number of failures caused by a RPC failure.
  first_time_download_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  first_time_download_scheduler->InvokeRequestCallback();
  ash::nearby::proto::ListPublicCertificatesResponse certificate_response;
  server_client_factory_.fake_server_client()
      ->InvokeListPublicCertificatesErrorCallback(
          ash::nearby::NearbyHttpError::kInternalServerError);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
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

  TriggerFirstTimeLocalCredentialUploadSuccess();

  // Simulate the scheduler notifying the CredentialManager that the download
  // task is ready when it has network connectivity.
  const raw_ptr<FakeNearbyScheduler, ExperimentalAsh>
      first_time_download_scheduler =
          scheduler_factory_.pref_name_to_on_demand_instance()
              .find(prefs::kNearbyPresenceSchedulingFirstTimeDownloadPrefName)
              ->second.fake_scheduler;
  first_time_download_scheduler->InvokeRequestCallback();

  // Simulate the max number of failures caused by a server response timeout.
  first_time_download_scheduler->SetNumConsecutiveFailures(
      kServerCommunicationMaxAttempts);
  task_environment_.FastForwardBy(kServerResponseTimeout);

  create_credential_manager_run_loop.Run();
  EXPECT_FALSE(credential_manager_);
}

}  // namespace ash::nearby::presence
