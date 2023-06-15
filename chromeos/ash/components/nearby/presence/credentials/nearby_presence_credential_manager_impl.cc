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
#include "chromeos/ash/components/nearby/presence/credentials/proto_conversions.h"
#include "chromeos/ash/components/nearby/presence/proto/list_public_certificates_rpc.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/rpc_resources.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/update_device_rpc.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/nearby/internal/proto/credential.pb.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";
const char kFirstTimeRegistrationFieldMaskPath[] = "display_name";
const char kUploadCredentialsFieldMaskPath[] = "certificates";
const base::TimeDelta kServerResponseTimeout = base::Seconds(5);
constexpr int kServerCommunicationMaxAttempts = 5;

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence)
    : NearbyPresenceCredentialManagerImpl(
          pref_service,
          identity_manager,
          url_loader_factory,
          nearby_presence,
          std::make_unique<LocalDeviceDataProviderImpl>(pref_service,
                                                        identity_manager)) {}

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
    std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider)
    : local_device_data_provider_(std::move(local_device_data_provider)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      nearby_presence_(nearby_presence),
      url_loader_factory_(url_loader_factory) {
  CHECK(pref_service_);
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
  CHECK(local_device_data_provider_);
}

NearbyPresenceCredentialManagerImpl::~NearbyPresenceCredentialManagerImpl() =
    default;

bool NearbyPresenceCredentialManagerImpl::IsLocalDeviceRegistered() {
  return local_device_data_provider_->IsRegistrationCompleteAndUserInfoSaved();
}

void NearbyPresenceCredentialManagerImpl::RegisterPresence(
    base::OnceCallback<void(bool)> on_registered_callback) {
  CHECK(!IsLocalDeviceRegistered());
  on_registered_callback_ = std::move(on_registered_callback);

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
  first_time_registration_on_demand_scheduler_->MakeImmediateRequest();
}

void NearbyPresenceCredentialManagerImpl::UpdateCredentials() {
  // TODO(b/276307539): Implement `UpdateCredentials`.
}

void NearbyPresenceCredentialManagerImpl::StartFirstTimeRegistration() {
  // The flow for first time registration is as follows:
  //      1. Register this device with the server.
  //      2. Generate this device's credentials.
  //      3. Upload this device's credentials.
  //      4. Download other devices' credentials.
  //      5. Save other devices' credentials.

  // Construct a request for first time registration to let the server know
  // to return the user's name and image url.
  ash::nearby::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(
      kDeviceIdPrefix + local_device_data_provider_->GetDeviceId());
  request.mutable_update_mask()->add_paths(kFirstTimeRegistrationFieldMaskPath);

  server_response_timer_.Start(
      FROM_HERE, kServerResponseTimeout,
      base::BindOnce(&NearbyPresenceCredentialManagerImpl::
                         HandleFirstTimeRegistrationTimeout,
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

void NearbyPresenceCredentialManagerImpl::HandleFirstTimeRegistrationTimeout() {
  // TODO(b/276307539): Add metrics to record the timeout.
  HandleFirstTimeRegistrationFailure();
}

void NearbyPresenceCredentialManagerImpl::HandleFirstTimeRegistrationFailure() {
  // TODO(b/276307539): Add metrics to record failures.
  server_client_.reset();

  // Allow the scheduler to exponentially attempt first time registration
  // until the max. Once it reaches the max attempts, notify consumers of
  // failure.
  if (first_time_registration_on_demand_scheduler_
          ->GetNumConsecutiveFailures() < kServerCommunicationMaxAttempts) {
    first_time_registration_on_demand_scheduler_->HandleResult(
        /*success=*/false);
    return;
  }

  // We've exceeded the max attempts; registration has failed.
  first_time_registration_on_demand_scheduler_->Stop();
  first_time_registration_on_demand_scheduler_.reset();
  CHECK(on_registered_callback_);
  std::move(on_registered_callback_).Run(/*success=*/false);
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

  // We've completed the 1st of 5 steps of first time registration:
  //   -> 1. Register this device with the server.
  //      2. Generate this device's credentials.
  //      3. Upload this device's credentials.
  //      4. Download other devices' credentials.
  //      5. Save other devices' credentials.
  // Next, kick off Step 2.
  nearby_presence_->UpdateLocalDeviceMetadataAndGenerateCredentials(
      MetadataToMojom(local_device_data_provider_->GetDeviceMetadata()),
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsGenerated,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnRegistrationRpcFailure(
    ash::nearby::NearbyHttpError error) {
  // TODO(b/276307539): Add metrics to record the type of NearbyHttpError.
  server_response_timer_.Stop();
  HandleFirstTimeRegistrationFailure();
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsGenerated(
    std::vector<mojom::SharedCredentialPtr> shared_credentials,
    mojom::StatusCode status) {
  if (status != mojom::StatusCode::kOk) {
    // TODO(b/276307539): Add metrics to record failures.
    CHECK(on_registered_callback_);
    std::move(on_registered_callback_).Run(/*success=*/false);
    return;
  }

  // With generated credentials, the CredentialManager needs to upload the
  // credentials to the server, and persist them to disk in order to detect
  // changes.
  std::vector<::nearby::internal::SharedCredential> proto_shared_credentials;
  for (const auto& cred : shared_credentials) {
    proto_shared_credentials.push_back(SharedCredentialFromMojom(cred.get()));
  }

  local_device_data_provider_->UpdatePersistedSharedCredentials(
      proto_shared_credentials);

  // We've completed the 2nd of 5 steps of first time registration:
  //      1. Register this device with the server.
  //   -> 2. Generate this device's credentials.
  //      3. Upload this device's credentials.
  //      4. Download other devices' credentials.
  //      5. Save other devices' credentials.
  // Next, kick off Step 3.
  ScheduleUploadCredentials(proto_shared_credentials);
}

void NearbyPresenceCredentialManagerImpl::ScheduleUploadCredentials(
    std::vector<::nearby::internal::SharedCredential>
        proto_shared_credentials) {
  first_time_upload_on_demand_scheduler_ =
      ash::nearby::NearbySchedulerFactory::CreateOnDemandScheduler(
          /*retry_failures=*/true,
          /*require_connectivity=*/true,
          prefs::kNearbyPresenceSchedulingFirstTimeUploadPrefName,
          pref_service_,
          base::BindRepeating(
              &NearbyPresenceCredentialManagerImpl::UploadCredentials,
              weak_ptr_factory_.GetWeakPtr(), proto_shared_credentials,
              base::BindRepeating(&NearbyPresenceCredentialManagerImpl::
                                      OnFirstTimeCredentialsUpload,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::DefaultClock::GetInstance());
  first_time_upload_on_demand_scheduler_->Start();
  first_time_upload_on_demand_scheduler_->MakeImmediateRequest();
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsUpload(
    bool success) {
  CHECK(first_time_upload_on_demand_scheduler_);
  if (!success) {
    // Allow the scheduler to exponentially attempt uploading credentials
    // until the max. Once it reaches the max attempts, notify consumers of
    // failure.
    if (first_time_upload_on_demand_scheduler_->GetNumConsecutiveFailures() <
        kServerCommunicationMaxAttempts) {
      first_time_upload_on_demand_scheduler_->HandleResult(
          /*success=*/false);
      return;
    }

    // We've exceeded the max attempts; registration has failed.
    first_time_upload_on_demand_scheduler_->Stop();
    first_time_upload_on_demand_scheduler_.reset();
    CHECK(on_registered_callback_);
    std::move(on_registered_callback_).Run(/*success=*/false);
    return;
  }

  first_time_upload_on_demand_scheduler_->HandleResult(/*success=*/true);
  first_time_upload_on_demand_scheduler_.reset();

  // We've completed the 3rd of 5 steps of first time registration:
  //      1. Register this device with the server.
  //      2. Generate this device's credentials.
  //   -> 3. Upload this device's credentials.
  //      4. Download other devices' credentials.
  //      5. Save other devices' credentials.
  // Next, kick off Step 4.
  ScheduleDownloadCredentials();
}

void NearbyPresenceCredentialManagerImpl::ScheduleDownloadCredentials() {
  // Next, to complete first time registration, the CredentialManager needs to
  // download the remote devices' shared credentials and save them to the
  // Nearby library.
  first_time_download_on_demand_scheduler_ =
      ash::nearby::NearbySchedulerFactory::CreateOnDemandScheduler(
          /*retry_failures=*/true,
          /*require_connectivity=*/true,
          prefs::kNearbyPresenceSchedulingFirstTimeDownloadPrefName,
          pref_service_,
          base::BindRepeating(
              &NearbyPresenceCredentialManagerImpl::DownloadCredentials,
              weak_ptr_factory_.GetWeakPtr(),
              base::BindRepeating(&NearbyPresenceCredentialManagerImpl::
                                      OnFirstTimeCredentialsDownload,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::DefaultClock::GetInstance());
  first_time_download_on_demand_scheduler_->Start();
  first_time_download_on_demand_scheduler_->MakeImmediateRequest();
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsDownload(
    std::vector<::nearby::internal::SharedCredential> credentials,
    bool success) {
  CHECK(first_time_download_on_demand_scheduler_);
  if (!success) {
    // Allow the scheduler to exponentially attempt uploading credentials
    // until the max. Once it reaches the max attempts, notify consumers of
    // failure.
    if (first_time_download_on_demand_scheduler_->GetNumConsecutiveFailures() <
        kServerCommunicationMaxAttempts) {
      first_time_download_on_demand_scheduler_->HandleResult(
          /*success=*/false);
      return;
    }

    // We've exceeded the max attempts; registration has failed.
    first_time_download_on_demand_scheduler_->Stop();
    first_time_download_on_demand_scheduler_.reset();
    CHECK(on_registered_callback_);
    std::move(on_registered_callback_).Run(/*success=*/false);
    return;
  }

  first_time_download_on_demand_scheduler_->HandleResult(/*success=*/true);
  first_time_download_on_demand_scheduler_.reset();

  // TODO(b/276307539): Currently first time registration is considered
  // successful on the successful download of remote credentials, however this
  // is not fully complete. Next, the CredentialManager needs to save the
  // download credentials to the NP library over the mojo pipe.
  //
  // We've completed the 4th of 5 steps for first time registration.
  //      1. Register this device with the server.
  //      2. Generate this device's credentials.
  //      3. Upload this device's credentials.
  //   -> 4. Download other devices' credentials.
  //      5. Save other devices' credentials.
  local_device_data_provider_->SetRegistrationComplete(/*complete=*/true);
  CHECK(on_registered_callback_);
  std::move(on_registered_callback_).Run(/*success=*/true);
}

void NearbyPresenceCredentialManagerImpl::UploadCredentials(
    std::vector<::nearby::internal::SharedCredential> credentials,
    base::RepeatingCallback<void(bool)> upload_credentials_result_callback) {
  ash::nearby::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(
      kDeviceIdPrefix + local_device_data_provider_->GetDeviceId());
  request.mutable_update_mask()->add_paths(kFirstTimeRegistrationFieldMaskPath);
  request.mutable_update_mask()->add_paths(kUploadCredentialsFieldMaskPath);

  std::vector<ash::nearby::proto::PublicCertificate> public_certificates;
  for (auto cred : credentials) {
    public_certificates.push_back(PublicCertificateFromSharedCredential(cred));
  }
  *(request.mutable_device()->mutable_public_certificates()) = {
      public_certificates.begin(), public_certificates.end()};

  server_response_timer_.Start(
      FROM_HERE, kServerResponseTimeout,
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnUploadCredentialsTimeout,
          weak_ptr_factory_.GetWeakPtr(), upload_credentials_result_callback));

  // Construct a HTTP client for the request. The HTTP client lifetime is
  // tied to a single request.
  CHECK(!server_client_);
  server_client_ = NearbyPresenceServerClientImpl::Factory::Create(
      std::make_unique<ash::nearby::NearbyApiCallFlowImpl>(), identity_manager_,
      url_loader_factory_);
  server_client_->UpdateDevice(
      request,
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnUploadCredentialsSuccess,
          weak_ptr_factory_.GetWeakPtr(), upload_credentials_result_callback),
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnUploadCredentialsFailure,
          weak_ptr_factory_.GetWeakPtr(), upload_credentials_result_callback));
}

void NearbyPresenceCredentialManagerImpl::HandleUploadCredentialsResult(
    base::RepeatingCallback<void(bool)> upload_credentials_callback,
    bool success) {
  // TODO(b/276307539): Add metrics to record success and failures.

  server_client_.reset();

  CHECK(upload_credentials_callback);
  upload_credentials_callback.Run(success);
}

void NearbyPresenceCredentialManagerImpl::OnUploadCredentialsTimeout(
    base::RepeatingCallback<void(bool)> upload_credentials_callback) {
  // TODO(b/276307539): Add metrics to record timeout.
  HandleUploadCredentialsResult(upload_credentials_callback,
                                /*success=*/false);
}

void NearbyPresenceCredentialManagerImpl::OnUploadCredentialsSuccess(
    base::RepeatingCallback<void(bool)> upload_credentials_callback,
    const ash::nearby::proto::UpdateDeviceResponse& response) {
  // TODO(b/276307539): Log response and check for changes in user name and
  // image url returned from the server.

  server_response_timer_.Stop();
  HandleUploadCredentialsResult(upload_credentials_callback,
                                /*success=*/true);
}

void NearbyPresenceCredentialManagerImpl::OnUploadCredentialsFailure(
    base::RepeatingCallback<void(bool)> upload_credentials_callback,
    ash::nearby::NearbyHttpError error) {
  // TODO(b/276307539): Add metrics to record the type of NearbyHttpError.

  server_response_timer_.Stop();
  HandleUploadCredentialsResult(upload_credentials_callback,
                                /*success=*/false);
}

void NearbyPresenceCredentialManagerImpl::DownloadCredentials(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback) {
  ash::nearby::proto::ListPublicCertificatesRequest request;
  request.set_parent(kDeviceIdPrefix +
                     local_device_data_provider_->GetDeviceId());

  server_response_timer_.Start(
      FROM_HERE, kServerResponseTimeout,
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsTimeout,
          weak_ptr_factory_.GetWeakPtr(),
          download_credentials_result_callback));

  // Construct a HTTP client for the request. The HTTP client lifetime is
  // tied to a single request.
  CHECK(!server_client_);
  server_client_ = NearbyPresenceServerClientImpl::Factory::Create(
      std::make_unique<ash::nearby::NearbyApiCallFlowImpl>(), identity_manager_,
      url_loader_factory_);
  server_client_->ListPublicCertificates(
      request,
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsSuccess,
          weak_ptr_factory_.GetWeakPtr(), download_credentials_result_callback),
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsFailure,
          weak_ptr_factory_.GetWeakPtr(),
          download_credentials_result_callback));
}

void NearbyPresenceCredentialManagerImpl::HandleDownloadCredentialsResult(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback,
    bool success,
    std::vector<::nearby::internal::SharedCredential> credentials) {
  // TODO(b/276307539): Add metrics to record failures.
  server_client_.reset();
  CHECK(download_credentials_result_callback);
  download_credentials_result_callback.Run(/*remote_credentials=*/credentials,
                                           /*success=*/success);
}

void NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsTimeout(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback) {
  // TODO(b/276307539): Add metrics to record timeout.
  HandleDownloadCredentialsResult(download_credentials_result_callback,
                                  /*success=*/false, /*credentials=*/{});
}

void NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsSuccess(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback,
    const ash::nearby::proto::ListPublicCertificatesResponse& response) {
  server_response_timer_.Stop();

  std::vector<::nearby::internal::SharedCredential> remote_credentials;
  for (auto public_certificate : response.public_certificates()) {
    remote_credentials.push_back(
        PublicCertificateToSharedCredential(public_certificate));
  }

  HandleDownloadCredentialsResult(download_credentials_result_callback,
                                  /*success=*/true,
                                  /*credentials=*/remote_credentials);
}

void NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsFailure(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback,
    ash::nearby::NearbyHttpError error) {
  // TODO(b/276307539): Add metrics to record the type of NearbyHttpError.
  server_response_timer_.Stop();
  HandleDownloadCredentialsResult(download_credentials_result_callback,
                                  /*success=*/false, /*credentials=*/{});
}

}  // namespace ash::nearby::presence
