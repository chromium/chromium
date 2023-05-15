// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow_impl.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_on_demand_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client_impl.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/ash/components/nearby/presence/proto/rpc_resources.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/update_device_rpc.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";
const char kFirstTimeRegistrationFieldMaskPath[] = "display_name";
const base::TimeDelta kServerResponseTimeout = base::Seconds(5);
constexpr int kServerRegistrationMaxAttempts = 5;

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // TODO(b/276307539): Add mojo remote as a parameter once implemented.
  NearbyPresenceCredentialManagerImpl(
      pref_service, identity_manager, url_loader_factory,
      std::make_unique<LocalDeviceDataProviderImpl>(pref_service,
                                                    identity_manager));
}

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider)
    : local_device_data_provider_(std::move(local_device_data_provider)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {
  // TODO(b/276307539): Add mojo remote as a parameter once implemented.
  CHECK(pref_service_);
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
  CHECK(local_device_data_provider_);

  first_time_registration_on_demand_scheduler_ =
      ash::nearby::NearbySchedulerFactory::CreateOnDemandScheduler(
          /*retry_failures=*/true,
          /*require_connectivity=*/true,
          prefs::kNearbyPresenceSchedulingFirstTimeRegistrationPrefName,
          pref_service_,
          base::BindRepeating(
              &NearbyPresenceCredentialManagerImpl::StartFirstTimeRegistration,
              weak_ptr_factory_.GetWeakPtr()),
          base::DefaultClock::GetInstance());
  first_time_registration_on_demand_scheduler_->Start();
}

NearbyPresenceCredentialManagerImpl::~NearbyPresenceCredentialManagerImpl() =
    default;

bool NearbyPresenceCredentialManagerImpl::IsLocalDeviceRegistered() {
  return local_device_data_provider_->IsUserRegistrationInfoSaved();
}

void NearbyPresenceCredentialManagerImpl::RegisterPresence(
    base::OnceCallback<void(bool)> on_registered_callback) {
  CHECK(!IsLocalDeviceRegistered());
  on_registered_callback_ = std::move(on_registered_callback);
  first_time_registration_on_demand_scheduler_->MakeImmediateRequest();
}

void NearbyPresenceCredentialManagerImpl::StartFirstTimeRegistration() {
  // Construct a request for first time registration to let the server know
  // to return the user's name and image url.
  ash::nearby::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(
      kDeviceIdPrefix + local_device_data_provider_->GetDeviceId());
  request.mutable_update_mask()->add_paths(kFirstTimeRegistrationFieldMaskPath);

  server_response_timer_.Start(
      FROM_HERE, kServerResponseTimeout,
      base::BindOnce(&NearbyPresenceCredentialManagerImpl::
                         HandleFirstTimeRegistrationFailure,
                     weak_ptr_factory_.GetWeakPtr()));

  // Construct a HTTP client for the request. The HTTP client lifetime is
  // tied to a single request.
  server_client_ = NearbyPresenceServerClientImpl::Factory::Create(
      std::make_unique<ash::nearby::NearbyApiCallFlowImpl>(), identity_manager_,
      url_loader_factory_);
  server_client_->UpdateDevice(
      request,
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnRegistrationRpcSuccess,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnRegistrationRpcFailure,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::HandleFirstTimeRegistrationFailure() {
  // TODO(b/276307539): Add metrics to record failures.

  server_client_.reset();

  // Allow the scheduler to exponentially attempt first time registration
  // until the max. Once it reaches the max attempts, notify consumers of
  // failure.
  if (kServerRegistrationMaxAttempts >=
      first_time_registration_on_demand_scheduler_
          ->GetNumConsecutiveFailures()) {
    first_time_registration_on_demand_scheduler_->Stop();
    CHECK(on_registered_callback_);
    std::move(on_registered_callback_).Run(/*success=*/false);
    return;
  }

  first_time_registration_on_demand_scheduler_->HandleResult(/*success=*/false);
}

void NearbyPresenceCredentialManagerImpl::OnRegistrationRpcSuccess(
    const ash::nearby::proto::UpdateDeviceResponse& response) {
  server_response_timer_.Stop();
  first_time_registration_on_demand_scheduler_->HandleResult(/*success=*/true);
  server_client_.reset();

  // Persist responses to be used to generate credentials.
  local_device_data_provider_->SaveUserRegistrationInfo(
      /*display_name=*/response.person_name(),
      /*image_url=*/response.image_url());

  // TODO(b/276307539): Currently first time registration is considered
  // successful on the return of the user's name and image url, however this
  // is not fully complete. Next, the CredentialManager needs to:
  // 1. Generate the credentials
  // 2. Upload the credentials
  // 3. Download the credentials
  // before executing the success callback.
  CHECK(on_registered_callback_);
  std::move(on_registered_callback_).Run(/*success=*/true);
}

void NearbyPresenceCredentialManagerImpl::OnRegistrationRpcFailure(
    ash::nearby::NearbyHttpError error) {
  // TODO(b/276307539): Add metrics to record the type of NearbyHttpError.
  server_response_timer_.Stop();
  HandleFirstTimeRegistrationFailure();
}

void NearbyPresenceCredentialManagerImpl::UpdateCredentials() {
  // TODO(b/276307539): Implement `UpdateCredentials`.
}

}  // namespace ash::nearby::presence
