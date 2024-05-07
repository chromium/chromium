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
#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client_impl.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/ash/components/nearby/presence/proto/list_shared_credentials_rpc.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/rpc_resources.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/update_device_rpc.pb.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/nearby/internal/proto/credential.pb.h"

namespace {

// static
bool g_is_credential_manager_set_for_testing_ = false;

const char kDeviceIdPrefix[] = "users/me/devices/";
const char kFirstTimeRegistrationFieldMaskPath[] = "display_name";
const char kUploadCredentialsFieldMaskPath[] = "certificates";
const base::TimeDelta kServerResponseTimeout = base::Seconds(5);
constexpr int kServerCommunicationMaxAttempts = 5;
const base::TimeDelta kSyncCredentialsDailyTimePeriod = base::Hours(24);
constexpr int kMaxUpdateCredentialRequestCount = 6;
std::vector<base::TimeDelta> kUpdateCredentialCoolDownPeriods = {
    base::Seconds(0), base::Seconds(15), base::Seconds(30), base::Minutes(1),
    base::Minutes(2), base::Minutes(5),  base::Minutes(10)};

bool HasCoolOffPeriodPassed(int update_credential_request_count,
                            base::Time last_daily_sync_success_time) {
  CHECK(update_credential_request_count <= kMaxUpdateCredentialRequestCount);
  return (base::Time::Now() - last_daily_sync_success_time) >=
         kUpdateCredentialCoolDownPeriods[update_credential_request_count];
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceCredentialManagerImpl::Creator::Creator() = default;
NearbyPresenceCredentialManagerImpl::Creator::~Creator() = default;

// static
NearbyPresenceCredentialManagerImpl::Creator*
NearbyPresenceCredentialManagerImpl::Creator::Get() {
  static base::NoDestructor<NearbyPresenceCredentialManagerImpl::Creator>
      creator;
  return creator.get();
}

void NearbyPresenceCredentialManagerImpl::Creator::Create(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
    CreateCallback on_created) {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Creating NearbyPresenceCredentialManager";
  Create(pref_service, identity_manager, url_loader_factory, nearby_presence,
         std::make_unique<LocalDeviceDataProviderImpl>(pref_service,
                                                       identity_manager),
         std::move(on_created));
}

void NearbyPresenceCredentialManagerImpl::Creator::Create(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
    std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider,
    CreateCallback on_created) {
  CHECK(!has_credential_manager_been_created_ ||
        g_is_credential_manager_set_for_testing_);
  has_credential_manager_been_created_ = true;

  on_created_ = (std::move(on_created));

  // This can only be set via `SetNextCredentialManagerInstanceForTesting`
  // since the class assumes only one CredentialManager is created per lifetime,
  // and asserts that there isn't an existing CredentialManager outside of unit
  // tests.
  if (g_is_credential_manager_set_for_testing_) {
    CHECK(credential_manager_under_initialization_);
    std::move(on_created_)
        .Run(std::move(credential_manager_under_initialization_));
    g_is_credential_manager_set_for_testing_ = false;
    return;
  }

  CHECK(!credential_manager_under_initialization_);
  credential_manager_under_initialization_ =
      base::WrapUnique(new NearbyPresenceCredentialManagerImpl(
          pref_service, identity_manager, url_loader_factory, nearby_presence,
          std::move(local_device_data_provider)));

  if (!credential_manager_under_initialization_->IsLocalDeviceRegistered()) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__
        << ": Device is not registered with server. "
           "Registering the local device.";
    credential_manager_under_initialization_->RegisterPresence(
        base::BindOnce(&NearbyPresenceCredentialManagerImpl::Creator::
                           OnCredentialManagerRegistered,
                       base::Unretained(this)));
    return;
  }

  credential_manager_under_initialization_->InitializeDeviceMetadata(
      base::BindOnce(&NearbyPresenceCredentialManagerImpl::Creator::
                         OnCredentialManagerInitialized,
                     base::Unretained(this)));
}

// static
void NearbyPresenceCredentialManagerImpl::Creator::
    SetNextCredentialManagerInstanceForTesting(
        std::unique_ptr<NearbyPresenceCredentialManager> credential_manager) {
  CHECK(credential_manager);
  CHECK(credential_manager->IsLocalDeviceRegistered());

  g_is_credential_manager_set_for_testing_ = true;
  Get()->credential_manager_under_initialization_ =
      std::move(credential_manager);
}

void NearbyPresenceCredentialManagerImpl::Creator::
    OnCredentialManagerRegistered(bool success) {
  CHECK(credential_manager_under_initialization_);
  CHECK(on_created_);

  if (!success) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": Credential manager failed to register.";
    // TODO(b/276307539): Add metrics to record failures.
    std::move(on_created_).Run(nullptr);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Credential manager successfully registered.";

  CHECK(credential_manager_under_initialization_->IsLocalDeviceRegistered());
  std::move(on_created_)
      .Run(std::move(credential_manager_under_initialization_));
  credential_manager_under_initialization_ = nullptr;
}

void NearbyPresenceCredentialManagerImpl::Creator::
    OnCredentialManagerInitialized() {
  CHECK(on_created_);
  CHECK(credential_manager_under_initialization_);
  CHECK(credential_manager_under_initialization_->IsLocalDeviceRegistered());

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Credential manager successfully initialized.";
  std::move(on_created_)
      .Run(std::move(credential_manager_under_initialization_));
}

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
    std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      local_device_data_provider_(std::move(local_device_data_provider)),
      nearby_presence_(nearby_presence),
      url_loader_factory_(url_loader_factory) {
  CHECK(pref_service_);
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
  CHECK(local_device_data_provider_);

  daily_credential_sync_scheduler_ =
      ash::nearby::NearbySchedulerFactory::CreatePeriodicScheduler(
          /*request_period=*/kSyncCredentialsDailyTimePeriod,
          /*retry_failures=*/true, /*require_connectivity=*/true,
          prefs::kNearbyPresenceSchedulingCredentialDailySyncPrefName,
          pref_service_,
          base::BindRepeating(
              &NearbyPresenceCredentialManagerImpl::StartDailySync,
              weak_ptr_factory_.GetWeakPtr()),
          Feature::NEARBY_INFRA, base::DefaultClock::GetInstance());
  daily_credential_sync_scheduler_->Start();
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
          Feature::NEARBY_INFRA, base::DefaultClock::GetInstance());
  first_time_registration_on_demand_scheduler_->Start();
  first_time_registration_on_demand_scheduler_->MakeImmediateRequest();
}

void NearbyPresenceCredentialManagerImpl::UpdateCredentials() {
  if (is_daily_sync_in_progress_) {
    return;
  }

  // Reset the request counter if we are at the max request count and the max
  // request cooloff period has passed.
  if (update_credential_request_count_ >= kMaxUpdateCredentialRequestCount) {
    CHECK(last_daily_sync_success_time_.has_value());
    if (HasCoolOffPeriodPassed(kMaxUpdateCredentialRequestCount,
                               last_daily_sync_success_time_.value())) {
      update_credential_request_count_ = 0;
    } else {
      // We're still in a cool-off period. Don't continue yet.
      return;
    }
  }

  // Trigger daily sync if:
  //   a. No daily sync has yet occurred during this profile session lifetime
  //      (signaled by `last_daily_sync_success_time_` being unset).
  //   b. The cool-off period between successful daily sync attempts has passed.
  if (!last_daily_sync_success_time_.has_value() ||
      HasCoolOffPeriodPassed(update_credential_request_count_,
                             last_daily_sync_success_time_.value())) {
    update_credential_request_count_++;
    daily_credential_sync_scheduler_->MakeImmediateRequest();
  }
}

void NearbyPresenceCredentialManagerImpl::InitializeDeviceMetadata(
    base::OnceClosure on_metadata_initialized_callback) {
  (*nearby_presence_)
      ->UpdateLocalDeviceMetadata(proto::MetadataToMojom(
          local_device_data_provider_->GetDeviceMetadata()));
  std::move(on_metadata_initialized_callback).Run();
}

void NearbyPresenceCredentialManagerImpl::StartFirstTimeRegistration() {
  // The flow for first time registration is as follows:
  //      1. Register this device with the server.
  //      2. Generate this device's credentials.
  //      3. Upload this device's credentials.
  //      4. Download other devices' credentials.
  //      5. Save other devices' credentials.

  first_time_server_registration_attempts_needed_count_++;

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Beginning first time registration.";

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
          weak_ptr_factory_.GetWeakPtr(),
          /*registration_request_start_time=*/base::TimeTicks::Now()),
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnRegistrationRpcFailure,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeRegistrationComplete(
    metrics::FirstTimeRegistrationResult result) {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": First time registration completed with result: ["
      << ((result == metrics::FirstTimeRegistrationResult::kSuccess)
              ? "success"
              : "failure")
      << "]";
  metrics::RecordFirstTimeRegistrationFlowResult(result);
  CHECK(on_registered_callback_);
  std::move(on_registered_callback_)
      .Run(/*success=*/(result ==
                        metrics::FirstTimeRegistrationResult::kSuccess));
}

void NearbyPresenceCredentialManagerImpl::HandleFirstTimeRegistrationTimeout() {
  HandleFirstTimeRegistrationFailure(
      /*result=*/ash::nearby::NearbyHttpResult::kTimeout);
}

void NearbyPresenceCredentialManagerImpl::HandleFirstTimeRegistrationFailure(
    ash::nearby::NearbyHttpResult result) {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Failed first time registration with result: " << result;

  server_client_.reset();
  metrics::RecordFirstTimeServerRegistrationFailureReason(result);

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
  first_time_server_registration_attempts_needed_count_ = 0;
  OnFirstTimeRegistrationComplete(
      metrics::FirstTimeRegistrationResult::kRegistrationWithServerFailure);
}

void NearbyPresenceCredentialManagerImpl::OnRegistrationRpcSuccess(
    base::TimeTicks registration_request_start_time,
    const ash::nearby::proto::UpdateDeviceResponse& response) {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__
      << ": Successfully registered device with the Nearby Presence server.";

  server_response_timer_.Stop();
  first_time_registration_on_demand_scheduler_->HandleResult(/*success=*/true);
  metrics::RecordFirstTimeServerRegistrationTotalAttemptsNeededCount(
      /*attempt_count=*/
      first_time_server_registration_attempts_needed_count_);
  first_time_server_registration_attempts_needed_count_ = 0;

  base::TimeDelta registration_duration =
      base::TimeTicks::Now() - registration_request_start_time;
  metrics::RecordFirstTimeServerRegistrationDuration(registration_duration);
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
  (*nearby_presence_)
      ->UpdateLocalDeviceMetadataAndGenerateCredentials(
          proto::MetadataToMojom(
              local_device_data_provider_->GetDeviceMetadata()),
          base::BindOnce(&NearbyPresenceCredentialManagerImpl::
                             OnFirstTimeCredentialsGenerated,
                         weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnRegistrationRpcFailure(
    ash::nearby::NearbyHttpError error) {
  server_response_timer_.Stop();
  HandleFirstTimeRegistrationFailure(
      /*result=*/ash::nearby::NearbyHttpErrorToResult(error));
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsGenerated(
    std::vector<mojom::SharedCredentialPtr> shared_credentials,
    mojo_base::mojom::AbslStatusCode status) {
  if (status != mojo_base::mojom::AbslStatusCode::kOk) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": First time credentials failed to generate.";
    OnFirstTimeRegistrationComplete(metrics::FirstTimeRegistrationResult::
                                        kLocalCredentialGenerationFailure);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": First time credentials successfully generated.";

  // With generated credentials, the CredentialManager needs to upload the
  // credentials to the server, and persist them to disk in order to detect
  // changes.
  std::vector<::nearby::internal::SharedCredential> proto_shared_credentials;
  for (const auto& cred : shared_credentials) {
    proto_shared_credentials.push_back(
        proto::SharedCredentialFromMojom(cred.get()));
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
  ScheduleUploadCredentials(
      proto_shared_credentials,
      base::BindRepeating(
          &NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsUpload,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsUpload(
    bool success) {
  if (!success) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << ": First time credential upload failed.";
    OnFirstTimeRegistrationComplete(
        metrics::FirstTimeRegistrationResult::kUploadLocalCredentialsFailure);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": First time credential upload succeeded.";

  // We've completed the 3rd of 5 steps of first time registration:
  //      1. Register this device with the server.
  //      2. Generate this device's credentials.
  //   -> 3. Upload this device's credentials.
  //      4. Download other devices' credentials.
  //      5. Save other devices' credentials.
  // Next, kick off Step 4.
  ScheduleDownloadCredentials(base::BindRepeating(
      &NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsDownload,
      weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeCredentialsDownload(
    std::vector<::nearby::internal::SharedCredential> credentials,
    bool success) {
  if (!success) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": First time credential download failed.";
    OnFirstTimeRegistrationComplete(metrics::FirstTimeRegistrationResult::
                                        kDownloadRemoteCredentialsFailure);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": First time credential download completed successfully.";

  // We've completed the 4th of 5 steps for first time registration.
  //      1. Register this device with the server.
  //      2. Generate this device's credentials.
  //      3. Upload this device's credentials.
  //   -> 4. Download other devices' credentials.
  //      5. Save other devices' credentials.
  // Next, kick off Step 5: save the remote shared credentials to the NP library
  // over mojo pipe.
  std::vector<mojom::SharedCredentialPtr> mojo_credentials;
  for (auto cred : credentials) {
    mojo_credentials.push_back(proto::SharedCredentialToMojom(cred));
  }

  (*nearby_presence_)
      ->UpdateRemoteSharedCredentials(
          std::move(mojo_credentials),
          local_device_data_provider_->GetAccountName(),
          base::BindOnce(&NearbyPresenceCredentialManagerImpl::
                             OnFirstTimeRemoteCredentialsSaved,
                         weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnFirstTimeRemoteCredentialsSaved(
    mojo_base::mojom::AbslStatusCode status) {
  if (status != mojo_base::mojom::AbslStatusCode::kOk) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": First time credential save failed.";
    OnFirstTimeRegistrationComplete(
        metrics::FirstTimeRegistrationResult::kSaveRemoteCredentialsFailure);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": First time credential save succeeded.";

  local_device_data_provider_->SetRegistrationComplete(/*complete=*/true);
  OnFirstTimeRegistrationComplete(
      metrics::FirstTimeRegistrationResult::kSuccess);
}

void NearbyPresenceCredentialManagerImpl::StartDailySync() {
  // If the device has not been registered yet, this is a no-op, and is
  // considered a success, which reschedules the daily sync. This happens when
  // the First Time Registration flow is kicked off, and the daily sync event
  // fires.
  if (!IsLocalDeviceRegistered()) {
    daily_credential_sync_scheduler_->HandleResult(/*success=*/true);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Beginning daily credential sync.";

  is_daily_sync_in_progress_ = true;

  // The flow for first time registration is as follows:
  //      1. Fetch this device's credentials.
  //      2. Upload this device's credentials if they have changed.
  //      3. Download other devices' credentials.
  //      4. Save other devices' credentials.
  //
  // Next, kick off Step 1.
  (*nearby_presence_)
      ->GetLocalSharedCredentials(
          local_device_data_provider_->GetAccountName(),
          base::BindOnce(
              &NearbyPresenceCredentialManagerImpl::OnGetLocalSharedCredentials,
              weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnGetLocalSharedCredentials(
    std::vector<mojom::SharedCredentialPtr> shared_credentials,
    mojo_base::mojom::AbslStatusCode status) {
  // On failures, exponentially retry the daily sync flow.
  if (status != mojo_base::mojom::AbslStatusCode::kOk) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": Failed to retrieve local shared credentials.";
    daily_credential_sync_scheduler_->HandleResult(/*success=*/false);
    return;
  }

  // We've completed the 1st of 4 steps for daily credential sync.
  //   -> 1. Fetch this device's credentials.
  //      2. Upload this device's credentials if they have changed.
  //      3. Download other devices' credentials.
  //      4. Save other devices' credentials.
  //
  // Next, kick off Step 2.

  // Convert from mojo to proto and check for changes for the credentials.
  // Only upload if they have changed, otherwise proceed to the next step.
  std::vector<::nearby::internal::SharedCredential> proto_shared_credentials;
  for (const auto& cred : shared_credentials) {
    proto_shared_credentials.push_back(
        proto::SharedCredentialFromMojom(cred.get()));
  }

  // Update the credentials persisted to disk if they have changed, and
  // schedule an upload of the credentials.
  if (local_device_data_provider_->HaveSharedCredentialsChanged(
          proto_shared_credentials)) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__
        << ": Persisted credentials have changed; scheduling upload of local "
           "updated credentials.";
    local_device_data_provider_->UpdatePersistedSharedCredentials(
        proto_shared_credentials);
    ScheduleUploadCredentials(
        proto_shared_credentials,
        base::BindRepeating(
            &NearbyPresenceCredentialManagerImpl::OnDailySyncCredentialUpload,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__
      << ": Persisted credentials have not changed; "
         "scheduling download of remote credentials.";

  // If the local credentials haven't changed, don't upload them to the server.
  // We've completed the 2nd of 4 steps for daily credential sync.
  //      1. Fetch this device's credentials.
  //  ->  2. Upload this device's credentials if they have changed.
  //      3. Download other devices' credentials.
  //      4. Save other devices' credentials.
  //
  // Next, kick off Step 3.
  ScheduleDownloadCredentials(base::BindRepeating(
      &NearbyPresenceCredentialManagerImpl::OnDailySyncCredentialDownload,
      weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnDailySyncCredentialUpload(
    bool success) {
  // On failures, exponentially retry the daily sync flow.
  if (!success) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": Failed to upload credentials.";
    daily_credential_sync_scheduler_->HandleResult(/*success=*/false);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Scheduling download of remote credentials.";

  // We've completed the 2nd of 4 steps for daily credential sync.
  //      1. Fetch this device's credentials.
  //  ->  2. Upload this device's credentials if they have changed.
  //      3. Download other devices' credentials.
  //      4. Save other devices' credentials.
  //
  // Next, kick off Step 3.
  ScheduleDownloadCredentials(base::BindRepeating(
      &NearbyPresenceCredentialManagerImpl::OnDailySyncCredentialDownload,
      weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnDailySyncCredentialDownload(
    std::vector<::nearby::internal::SharedCredential> credentials,
    bool success) {
  // On failures, exponentially retry the daily sync flow.
  if (!success) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": Failed to download remote credentials.";
    daily_credential_sync_scheduler_->HandleResult(/*success=*/false);
    return;
  }

  // We've completed the 3rd of 4 steps for daily credential sync.
  //      1. Fetch this device's credentials.
  //      2. Upload this device's credentials if they have changed.
  //  ->  3. Download other devices' credentials.
  //      4. Save other devices' credentials.
  //
  // Next, kick off Step 4.
  //
  // Convert the credentials and send them over the mojo pipe to the NP library.
  std::vector<mojom::SharedCredentialPtr> mojo_credentials;
  for (auto cred : credentials) {
    mojo_credentials.push_back(proto::SharedCredentialToMojom(cred));
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__
      << ": Beginning attempt to save remote credentials "
         "to credential storage.";

  (*nearby_presence_)
      ->UpdateRemoteSharedCredentials(
          std::move(mojo_credentials),
          local_device_data_provider_->GetAccountName(),
          base::BindOnce(&NearbyPresenceCredentialManagerImpl::
                             OnDailySyncRemoteCredentialsSaved,
                         weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialManagerImpl::OnDailySyncRemoteCredentialsSaved(
    mojo_base::mojom::AbslStatusCode status) {
  // On failures, exponentially retry the daily sync flow.
  if (status != mojo_base::mojom::AbslStatusCode::kOk) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__
        << ": Failed to save remote credentials to credential storage.";
    daily_credential_sync_scheduler_->HandleResult(/*success=*/false);
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__
      << ": Successfully stored remote credentials to credential storage.";

  // We've completed the last of 4 steps for daily credential sync.
  //      1. Fetch this device's credentials.
  //      2. Upload this device's credentials if they have changed.
  //      3. Download other devices' credentials.
  //   -> 4. Save other devices' credentials.
  // Signal success to the scheduler, which causes it to reschedule for the
  // next daily sync.
  daily_credential_sync_scheduler_->HandleResult(/*success=*/true);

  is_daily_sync_in_progress_ = false;
  last_daily_sync_success_time_ = base::Time::Now();
}

void NearbyPresenceCredentialManagerImpl::ScheduleUploadCredentials(
    std::vector<::nearby::internal::SharedCredential> proto_shared_credentials,
    base::RepeatingCallback<void(bool)> on_upload) {
  upload_on_demand_scheduler_ =
      ash::nearby::NearbySchedulerFactory::CreateOnDemandScheduler(
          /*retry_failures=*/true,
          /*require_connectivity=*/true,
          prefs::kNearbyPresenceSchedulingUploadPrefName, pref_service_,
          base::BindRepeating(
              &NearbyPresenceCredentialManagerImpl::UploadCredentials,
              weak_ptr_factory_.GetWeakPtr(), proto_shared_credentials,
              std::move(on_upload)),
          Feature::NEARBY_INFRA, base::DefaultClock::GetInstance());
  upload_on_demand_scheduler_->Start();
  upload_on_demand_scheduler_->MakeImmediateRequest();
}

void NearbyPresenceCredentialManagerImpl::ScheduleDownloadCredentials(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        on_download) {
  download_on_demand_scheduler_ =
      ash::nearby::NearbySchedulerFactory::CreateOnDemandScheduler(
          /*retry_failures=*/true,
          /*require_connectivity=*/true,
          prefs::kNearbyPresenceSchedulingDownloadPrefName, pref_service_,
          base::BindRepeating(
              &NearbyPresenceCredentialManagerImpl::DownloadCredentials,
              weak_ptr_factory_.GetWeakPtr(), std::move(on_download)),
          Feature::NEARBY_INFRA, base::DefaultClock::GetInstance());
  download_on_demand_scheduler_->Start();
  download_on_demand_scheduler_->MakeImmediateRequest();
}

void NearbyPresenceCredentialManagerImpl::UploadCredentials(
    std::vector<::nearby::internal::SharedCredential> credentials,
    base::RepeatingCallback<void(bool)> upload_credentials_result_callback) {
  upload_credentials_attempts_needed_count_++;
  ash::nearby::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(
      kDeviceIdPrefix + local_device_data_provider_->GetDeviceId());
  request.mutable_update_mask()->add_paths(kFirstTimeRegistrationFieldMaskPath);
  request.mutable_update_mask()->add_paths(kUploadCredentialsFieldMaskPath);

  std::vector<ash::nearby::proto::PublicCertificate> public_certificates;
  for (auto cred : credentials) {
    public_certificates.push_back(
        proto::PublicCertificateFromSharedCredential(cred));
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
          weak_ptr_factory_.GetWeakPtr(), upload_credentials_result_callback,
          /*upload_request_start_time=*/base::TimeTicks::Now()),
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnUploadCredentialsFailure,
          weak_ptr_factory_.GetWeakPtr(), upload_credentials_result_callback));
}

void NearbyPresenceCredentialManagerImpl::HandleUploadCredentialsResult(
    base::RepeatingCallback<void(bool)> upload_credentials_callback,
    ash::nearby::NearbyHttpResult result) {
  server_client_.reset();

  CHECK(upload_on_demand_scheduler_);
  if (result != ash::nearby::NearbyHttpResult::kSuccess) {
    metrics::RecordSharedCredentialUploadAttemptFailureReason(result);

    // Allow the scheduler to exponentially attempt uploading credentials
    // until the max. Once it reaches the max attempts, notify consumers of
    // failure.
    if (upload_on_demand_scheduler_->GetNumConsecutiveFailures() <
        kServerCommunicationMaxAttempts) {
      upload_on_demand_scheduler_->HandleResult(
          /*success=*/false);
      return;
    }

    // We've exceeded the max attempts; registration has failed.
    upload_on_demand_scheduler_->Stop();
    metrics::RecordSharedCredentialUploadResult(/*success=*/false);
    upload_on_demand_scheduler_.reset();
    CHECK(upload_credentials_callback);
    upload_credentials_callback.Run(/*success=*/false);
    upload_credentials_attempts_needed_count_ = 0;
    return;
  }

  upload_on_demand_scheduler_->HandleResult(/*success=*/true);
  metrics::RecordSharedCredentialUploadResult(/*success=*/true);
  metrics::RecordSharedCredentialUploadTotalAttemptsNeededCount(
      upload_credentials_attempts_needed_count_);
  upload_on_demand_scheduler_.reset();
  upload_credentials_attempts_needed_count_ = 0;
  CHECK(upload_credentials_callback);
  upload_credentials_callback.Run(/*success=*/true);
}

void NearbyPresenceCredentialManagerImpl::OnUploadCredentialsTimeout(
    base::RepeatingCallback<void(bool)> upload_credentials_callback) {
  HandleUploadCredentialsResult(
      upload_credentials_callback,
      /*result=*/ash::nearby::NearbyHttpResult::kTimeout);
}

void NearbyPresenceCredentialManagerImpl::OnUploadCredentialsSuccess(
    base::RepeatingCallback<void(bool)> upload_credentials_callback,
    base::TimeTicks upload_request_start_time,
    const ash::nearby::proto::UpdateDeviceResponse& response) {
  // TODO(b/276307539): Log response and check for changes in user name and
  // image url returned from the server.

  server_response_timer_.Stop();
  base::TimeDelta upload_request_duration =
      base::TimeTicks::Now() - upload_request_start_time;
  metrics::RecordSharedCredentialUploadDuration(upload_request_duration);
  HandleUploadCredentialsResult(
      upload_credentials_callback,
      /*result=*/ash::nearby::NearbyHttpResult::kSuccess);
}

void NearbyPresenceCredentialManagerImpl::OnUploadCredentialsFailure(
    base::RepeatingCallback<void(bool)> upload_credentials_callback,
    ash::nearby::NearbyHttpError error) {
  server_response_timer_.Stop();
  HandleUploadCredentialsResult(
      upload_credentials_callback,
      /*result=*/ash::nearby::NearbyHttpErrorToResult(error));
}

void NearbyPresenceCredentialManagerImpl::DownloadCredentials(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback) {
  download_credentials_attempts_needed_count_++;
  ash::nearby::proto::ListSharedCredentialsRequest request;

  // TODO(hansberry): Populate with actual DUSI.
  request.set_dusi("test_dusi");
  request.set_identity_type(
      ash::nearby::proto::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);

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
  server_client_->ListSharedCredentials(
      request,
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsSuccess,
          weak_ptr_factory_.GetWeakPtr(), download_credentials_result_callback,
          /*download_request_start_time=*/base::TimeTicks::Now()),
      base::BindOnce(
          &NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsFailure,
          weak_ptr_factory_.GetWeakPtr(),
          download_credentials_result_callback));
}

void NearbyPresenceCredentialManagerImpl::HandleDownloadCredentialsResult(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback,
    ash::nearby::NearbyHttpResult result,
    std::vector<::nearby::internal::SharedCredential> credentials) {
  server_client_.reset();

  CHECK(download_on_demand_scheduler_);
  if (result != ash::nearby::NearbyHttpResult::kSuccess) {
    metrics::RecordSharedCredentialDownloadFailureReason(result);

    // Allow the scheduler to exponentially attempt downloading credentials
    // until the max. Once it reaches the max attempts, notify consumers of
    // failure.
    if (download_on_demand_scheduler_->GetNumConsecutiveFailures() <
        kServerCommunicationMaxAttempts) {
      download_on_demand_scheduler_->HandleResult(
          /*success=*/false);
      return;
    }

    // We've exceeded the max attempts; registration has failed.
    download_on_demand_scheduler_->Stop();
    metrics::RecordSharedCredentialDownloadResult(/*success=*/false);
    download_on_demand_scheduler_.reset();
    CHECK(download_credentials_result_callback);
    download_credentials_result_callback.Run(/*credentials=*/{},
                                             /*success=*/false);
    download_credentials_attempts_needed_count_ = 0;
    return;
  }

  download_on_demand_scheduler_->HandleResult(/*success=*/true);
  metrics::RecordSharedCredentialDownloadResult(/*success=*/true);
  metrics::RecordSharedCredentialDownloadTotalAttemptsNeededCount(
      download_credentials_attempts_needed_count_);
  download_on_demand_scheduler_.reset();
  download_credentials_attempts_needed_count_ = 0;
  CHECK(download_credentials_result_callback);
  download_credentials_result_callback.Run(/*credentials=*/credentials,
                                           /*success=*/true);
}

void NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsTimeout(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback) {
  HandleDownloadCredentialsResult(
      download_credentials_result_callback,
      /*result=*/ash::nearby::NearbyHttpResult::kTimeout, /*credentials=*/{});
}

void NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsSuccess(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback,
    base::TimeTicks download_request_start_time,
    const ash::nearby::proto::ListSharedCredentialsResponse& response) {
  server_response_timer_.Stop();
  base::TimeDelta download_request_duration =
      base::TimeTicks::Now() - download_request_start_time;
  metrics::RecordSharedCredentialDownloadDuration(download_request_duration);

  std::vector<::nearby::internal::SharedCredential> shared_credentials;
  for (auto remote_shared_credential : response.shared_credentials()) {
    shared_credentials.push_back(
        proto::RemoteSharedCredentialToThirdPartySharedCredential(
            remote_shared_credential));
  }

  HandleDownloadCredentialsResult(
      download_credentials_result_callback,
      /*result=*/ash::nearby::NearbyHttpResult::kSuccess,
      /*credentials=*/shared_credentials);
}

void NearbyPresenceCredentialManagerImpl::OnDownloadCredentialsFailure(
    base::RepeatingCallback<
        void(std::vector<::nearby::internal::SharedCredential>, bool)>
        download_credentials_result_callback,
    ash::nearby::NearbyHttpError error) {
  server_response_timer_.Stop();
  HandleDownloadCredentialsResult(
      download_credentials_result_callback,
      /*result=*/ash::nearby::NearbyHttpErrorToResult(error),
      /*credentials=*/{});
}

}  // namespace ash::nearby::presence
