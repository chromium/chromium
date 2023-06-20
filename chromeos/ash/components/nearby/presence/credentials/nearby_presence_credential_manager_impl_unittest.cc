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

const std::string kUserName = "Test Tester";
const std::string kDeviceId = "0123456789";
const std::string kProfileUrl = "https://example.com";
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

}  // namespace

namespace ash::nearby::presence {

class TestNearbyPresenceCredentialManagerImpl
    : public NearbyPresenceCredentialManagerImpl {
 public:
  TestNearbyPresenceCredentialManagerImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
          nearby_presence,
      std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider)
      : NearbyPresenceCredentialManagerImpl(
            pref_service,
            identity_manager,
            url_loader_factory,
            nearby_presence,
            std::move(local_device_data_provider)) {}
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
    std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider =
        std::make_unique<FakeLocalDeviceDataProvider>();
    fake_local_device_data_provider_ =
        static_cast<FakeLocalDeviceDataProvider*>(
            local_device_data_provider.get());

    credential_manager_ =
        std::make_unique<TestNearbyPresenceCredentialManagerImpl>(
            &pref_service_, identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            fake_nearby_presence_.shared_remote(),
            std::move(local_device_data_provider));

    // Simulate first time registration flow.
    fake_local_device_data_provider_->SetRegistrationComplete(false);
    EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());

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

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeNearbyPresence fake_nearby_presence_;
  raw_ptr<FakeLocalDeviceDataProvider, ExperimentalAsh>
      fake_local_device_data_provider_ = nullptr;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeNearbyPresenceServerClient::Factory server_client_factory_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
  std::unique_ptr<NearbyPresenceCredentialManager> credential_manager_;
};

TEST_F(NearbyPresenceCredentialManagerImplTest, RegistrationSuccess) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      run_loop.QuitClosure());

  // Expect success on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(true)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  run_loop.Run();

  TriggerFirstTimeLocalCredentialUploadSuccess();

  TriggerFirstTimeDownloadRemoteCredentialSuccess();

  EXPECT_TRUE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationTimeout) {
  // Expect failure on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

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

  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationFailure) {
  // Expect failure on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

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

  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, CredentialGenerationFailure) {
  // Simulate the credentials being failed to be generated in the NP library.
  fake_nearby_presence_.SetGenerateCredentialsResponse(
      {}, mojom::StatusCode::kFailure);

  // Expect failure on the callback.
  base::RunLoop run_loop;
  credential_manager_->RegisterPresence(
      base::BindLambdaForTesting([&](bool result) {
        EXPECT_FALSE(result);
        run_loop.Quit();
      }));

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  run_loop.Run();

  // Since the user registration with the server was not successful, this will
  // be false.
  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest,
       UploadCredentialsServerTimeout) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      run_loop.QuitClosure());

  // Expect failure on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  run_loop.Run();

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

  // Since the user registration with the server was not successful, this will
  // be false.
  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, UploadCredentialsFailure) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      run_loop.QuitClosure());

  // Expect failure on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  run_loop.Run();

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

  // Since the user registration with the server was not successful, this will
  // be false.
  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, DownloadCredentialsFailure) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      run_loop.QuitClosure());

  // Expect success on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  TriggerFirstTimeRegistrationSuccess();
  // Required for credentials to be generated and passed over the mojo pipe.
  run_loop.Run();

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

  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, DownloadCredentialsTimeout) {
  // Wait until after the generated credentials are saved to continue the test.
  base::RunLoop run_loop;
  fake_local_device_data_provider_->SetUpdatePersistedSharedCredentialsCallback(
      run_loop.QuitClosure());

  // Expect success on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  TriggerFirstTimeRegistrationSuccess();

  // Required for credentials to be generated and passed over the mojo pipe.
  run_loop.Run();

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

  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

}  // namespace ash::nearby::presence
