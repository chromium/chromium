// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_device_manager_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_logging.h"
#include "chromeos/ash/services/device_sync/synced_bluetooth_address_tracker_impl.h"

namespace ash {

namespace device_sync {

namespace {

void RecordDeviceSyncResult(CryptAuthDeviceSyncResult result) {
  base::UmaHistogramEnumeration("CryptAuth.DeviceSyncV2.Result.ResultType",
                                result.GetResultType());
  base::UmaHistogramEnumeration("CryptAuth.DeviceSyncV2.Result.ResultCode",
                                result.result_code());
  base::UmaHistogramBoolean(
      "CryptAuth.DeviceSyncV2.Result.DidDeviceRegistryChange",
      result.did_device_registry_change());
}

}  // namespace

// static
CryptAuthV2DeviceManagerImpl::Factory*
    CryptAuthV2DeviceManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthV2DeviceManager>
CryptAuthV2DeviceManagerImpl::Factory::Create(
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    PrefService* pref_service,
    AttestationCertificatesSyncer::GetAttestationCertificatesFunction
        get_attestation_certificates_function) {
  if (test_factory_) {
    return test_factory_->CreateInstance(client_app_metadata, device_registry,
                                         key_registry, client_factory,
                                         gcm_manager, scheduler, pref_service,
                                         get_attestation_certificates_function);
  }

  return base::WrapUnique(new CryptAuthV2DeviceManagerImpl(
      client_app_metadata, device_registry, key_registry, client_factory,
      gcm_manager, scheduler, pref_service,
      get_attestation_certificates_function));
}

// static
void CryptAuthV2DeviceManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthV2DeviceManagerImpl::Factory::~Factory() = default;

CryptAuthV2DeviceManagerImpl::CryptAuthV2DeviceManagerImpl(
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    PrefService* pref_service,
    AttestationCertificatesSyncer::GetAttestationCertificatesFunction
        get_attestation_certificates_function)
    : synced_bluetooth_address_tracker_(
          SyncedBluetoothAddressTrackerImpl::Factory::Create(scheduler,
                                                             pref_service)),
      attestation_certificates_syncer_(
          AttestationCertificatesSyncerImpl::Factory::Create(
              scheduler,
              pref_service,
              get_attestation_certificates_function)),
      client_app_metadata_(client_app_metadata),
      device_registry_(device_registry),
      key_registry_(key_registry),
      client_factory_(client_factory),
      gcm_manager_(gcm_manager),
      scheduler_(scheduler),
      pref_service_(pref_service) {
  gcm_manager_->AddObserver(this);
}

CryptAuthV2DeviceManagerImpl::~CryptAuthV2DeviceManagerImpl() {
  gcm_manager_->RemoveObserver(this);
}

void CryptAuthV2DeviceManagerImpl::Start() {
  PA_LOG(VERBOSE)
      << "Starting CryptAuth v2 device manager with device registry:\n"
      << *device_registry_;

  scheduler_->StartDeviceSyncScheduling(
      scheduler_weak_ptr_factory_.GetWeakPtr());
}

const CryptAuthDeviceRegistry::InstanceIdToDeviceMap&
CryptAuthV2DeviceManagerImpl::GetSyncedDevices() const {
  return device_registry_->instance_id_to_device_map();
}

void CryptAuthV2DeviceManagerImpl::ForceDeviceSyncNow(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const std::optional<std::string>& session_id) {
  scheduler_->RequestDeviceSync(invocation_reason, session_id);
}

BetterTogetherMetadataStatus
CryptAuthV2DeviceManagerImpl::GetDeviceSyncerBetterTogetherMetadataStatus()
    const {
  if (!device_syncer_) {
    return BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet;
  }

  return device_syncer_->better_together_metadata_status();
}

GroupPrivateKeyStatus
CryptAuthV2DeviceManagerImpl::GetDeviceSyncerGroupPrivateKeyStatus() const {
  if (!device_syncer_) {
    return GroupPrivateKeyStatus::kStatusUnavailableBecauseNoDeviceSyncerSet;
  }

  return device_syncer_->group_private_key_status();
}

std::optional<base::Time> CryptAuthV2DeviceManagerImpl::GetLastDeviceSyncTime()
    const {
  return scheduler_->GetLastSuccessfulDeviceSyncTime();
}

std::optional<base::TimeDelta>
CryptAuthV2DeviceManagerImpl::GetTimeToNextAttempt() const {
  return scheduler_->GetTimeToNextDeviceSyncRequest();
}

bool CryptAuthV2DeviceManagerImpl::IsDeviceSyncInProgress() const {
  return scheduler_->IsWaitingForDeviceSyncResult();
}

bool CryptAuthV2DeviceManagerImpl::IsRecoveringFromFailure() const {
  return scheduler_->GetNumConsecutiveDeviceSyncFailures() > 0;
}

void CryptAuthV2DeviceManagerImpl::OnDeviceSyncRequested(
    const cryptauthv2::ClientMetadata& client_metadata) {
  NotifyDeviceSyncStarted(client_metadata);

  current_client_metadata_ = client_metadata;

  base::UmaHistogramExactLinear(
      "CryptAuth.DeviceSyncV2.InvocationReason",
      current_client_metadata_->invocation_reason(),
      cryptauthv2::ClientMetadata::InvocationReason_ARRAYSIZE);

  PA_LOG(VERBOSE) << "Starting CryptAuth v2 DeviceSync.";
  device_syncer_ = CryptAuthDeviceSyncerImpl::Factory::Create(
      device_registry_, key_registry_, client_factory_,
      synced_bluetooth_address_tracker_.get(),
      attestation_certificates_syncer_.get(), pref_service_);
  device_syncer_->Sync(
      *current_client_metadata_, client_app_metadata_,
      base::BindOnce(&CryptAuthV2DeviceManagerImpl::OnDeviceSyncFinished,
                     base::Unretained(this)));
}

void CryptAuthV2DeviceManagerImpl::OnResyncMessage(
    const std::optional<std::string>& session_id,
    const std::optional<CryptAuthFeatureType>& feature_type) {
  PA_LOG(VERBOSE) << "Received GCM message to re-sync devices (session ID: "
                  << session_id.value_or("[No session ID]") << ").";

  ForceDeviceSyncNow(cryptauthv2::ClientMetadata::SERVER_INITIATED, session_id);
}

void CryptAuthV2DeviceManagerImpl::OnDeviceSyncFinished(
    CryptAuthDeviceSyncResult device_sync_result) {
  device_syncer_.reset();

  std::stringstream prefix;
  prefix << "DeviceSync attempt with invocation reason "
         << current_client_metadata_->invocation_reason();
  std::stringstream suffix;
  suffix << "with result code " << device_sync_result.result_code() << ".";
  switch (device_sync_result.GetResultType()) {
    case CryptAuthDeviceSyncResult::ResultType::kSuccess:
      PA_LOG(INFO) << prefix.str() << " succeeded  " << suffix.str();
      break;
    case CryptAuthDeviceSyncResult::ResultType::kNonFatalError:
      PA_LOG(WARNING) << prefix.str() << " finished with non-fatal errors "
                      << suffix.str();
      break;
    case CryptAuthDeviceSyncResult::ResultType::kFatalError:
      PA_LOG(ERROR) << prefix.str() << " failed " << suffix.str();
      break;
  }

  PA_LOG(INFO) << "The device registry "
               << (device_sync_result.did_device_registry_change()
                       ? "changed."
                       : "did not change.");
  PA_LOG(VERBOSE) << "Device registry:\n" << *device_registry_;

  current_client_metadata_.reset();

  RecordDeviceSyncResult(device_sync_result);

  scheduler_->HandleDeviceSyncResult(device_sync_result);

  std::optional<base::TimeDelta> time_to_next_attempt = GetTimeToNextAttempt();
  if (time_to_next_attempt) {
    PA_LOG(INFO) << "Time until next DeviceSync attempt: "
                 << *time_to_next_attempt << ".";
  } else {
    PA_LOG(INFO) << "No future DeviceSync requests currently scheduled.";
  }

  if (!device_sync_result.IsSuccess()) {
    PA_LOG(INFO) << "Number of consecutive DeviceSync failures: "
                 << scheduler_->GetNumConsecutiveDeviceSyncFailures() << ".";
  }

  NotifyDeviceSyncFinished(device_sync_result);
}

}  // namespace device_sync

}  // namespace ash
