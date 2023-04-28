// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"

#include "base/functional/callback_helpers.h"
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
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kUserName = "Test Tester";
const std::string kDeviceId = "0123456789";
const std::string kProfileUrl = "https://example.com";
const base::TimeDelta kServerResponseTimeout = base::Seconds(5);
constexpr int kServerRegistrationMaxRetries = 5;

}  // namespace

namespace ash::nearby::presence {

class TestNearbyPresenceCredentialManagerImpl
    : public NearbyPresenceCredentialManagerImpl {
 public:
  TestNearbyPresenceCredentialManagerImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider)
      : NearbyPresenceCredentialManagerImpl(
            pref_service,
            identity_manager,
            url_loader_factory,
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
            std::move(local_device_data_provider));

    first_time_registration_scheduler_ =
        scheduler_factory_.pref_name_to_on_demand_instance()
            .find(prefs::kNearbyPresenceSchedulingFirstTimeRegistrationPrefName)
            ->second.fake_scheduler;
  }

  void TearDown() override {
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyPresenceServerClientImpl::Factory::SetFactoryForTesting(nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ash::nearby::FakeNearbyScheduler* first_time_registration_scheduler_ =
      nullptr;
  FakeLocalDeviceDataProvider* fake_local_device_data_provider_ = nullptr;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeNearbyPresenceServerClient::Factory server_client_factory_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
  std::unique_ptr<NearbyPresenceCredentialManager> credential_manager_;
};

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationSuccess) {
  // Simulate first time registration flow.
  fake_local_device_data_provider_->SetIsUserRegistrationInfoSaved(false);
  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());

  // Simulate the device id which will be generated in a call to |GetDeviceId|.
  fake_local_device_data_provider_->SetDeviceId(kDeviceId);

  // Expect success on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(true)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  // Simulate the scheduler notifying the CredentialManager that the task is
  // ready when it has network connectivity.
  first_time_registration_scheduler_->InvokeRequestCallback();

  // Mock and return the server response.
  ash::nearby::proto::UpdateDeviceResponse response;
  response.set_person_name(kUserName);
  response.set_image_url(kProfileUrl);
  server_client_factory_.fake_server_client()
      ->InvokeUpdateDeviceSuccessCallback(response);

  EXPECT_TRUE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationTimeout) {
  // Simulate first time registration flow.
  fake_local_device_data_provider_->SetIsUserRegistrationInfoSaved(false);
  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());

  // Simulate the device id which will be generated in a call to |GetDeviceId|.
  fake_local_device_data_provider_->SetDeviceId(kDeviceId);

  // Expect failure on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  // Simulate the max number of failures caused by a server response timeout.
  first_time_registration_scheduler_->SetNumConsecutiveFailures(
      kServerRegistrationMaxRetries);
  first_time_registration_scheduler_->InvokeRequestCallback();
  task_environment_.FastForwardBy(kServerResponseTimeout);

  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

TEST_F(NearbyPresenceCredentialManagerImplTest, ServerRegistrationFailure) {
  // Simulate first time registration flow.
  fake_local_device_data_provider_->SetIsUserRegistrationInfoSaved(false);
  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());

  // Simulate the device id which will be generated in a call to |GetDeviceId|.
  fake_local_device_data_provider_->SetDeviceId(kDeviceId);

  // Expect failure on the callback.
  base::MockCallback<base::OnceCallback<void(bool)>>
      on_registered_mock_callback;
  EXPECT_CALL(on_registered_mock_callback, Run(false)).Times(1);
  credential_manager_->RegisterPresence(on_registered_mock_callback.Get());

  // Simulate the max number of failures caused by a RPC failure.
  first_time_registration_scheduler_->SetNumConsecutiveFailures(
      kServerRegistrationMaxRetries);
  first_time_registration_scheduler_->InvokeRequestCallback();
  server_client_factory_.fake_server_client()->InvokeUpdateDeviceErrorCallback(
      ash::nearby::NearbyHttpError::kInternalServerError);

  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());
}

}  // namespace ash::nearby::presence
