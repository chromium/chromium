// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_v2_device_manager_impl.h"

#include <sstream>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_device_syncer_impl.h"
#include "chromeos/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/services/device_sync/public/cpp/client_app_metadata_provider.h"

namespace chromeos {

namespace device_sync {

namespace {

// Timeout value for asynchronous operation.
// TODO(https://crbug.com/933656): Tune these values.
constexpr base::TimeDelta kWaitingForClientAppMetadataTimeout =
    base::TimeDelta::FromSeconds(10);

}  // namespace

// static
CryptAuthV2DeviceManagerImpl::Factory*
    CryptAuthV2DeviceManagerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthV2DeviceManagerImpl::Factory*
CryptAuthV2DeviceManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthV2DeviceManagerImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthV2DeviceManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthV2DeviceManagerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthV2DeviceManager>
CryptAuthV2DeviceManagerImpl::Factory::BuildInstance(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new CryptAuthV2DeviceManagerImpl(
      client_app_metadata_provider, device_registry, key_registry,
      client_factory, gcm_manager, scheduler, std::move(timer)));
}

CryptAuthV2DeviceManagerImpl::CryptAuthV2DeviceManagerImpl(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_app_metadata_provider_(client_app_metadata_provider),
      device_registry_(device_registry),
      key_registry_(key_registry),
      client_factory_(client_factory),
      gcm_manager_(gcm_manager),
      scheduler_(scheduler),
      timer_(std::move(timer)) {
  gcm_manager_->AddObserver(this);
}

CryptAuthV2DeviceManagerImpl::~CryptAuthV2DeviceManagerImpl() {
  gcm_manager_->RemoveObserver(this);
}

void CryptAuthV2DeviceManagerImpl::Start() {
  scheduler_->StartDeviceSyncScheduling(
      scheduler_weak_ptr_factory_.GetWeakPtr());
}

const CryptAuthDeviceRegistry::InstanceIdToDeviceMap&
CryptAuthV2DeviceManagerImpl::GetSyncedDevices() const {
  return device_registry_->instance_id_to_device_map();
}

void CryptAuthV2DeviceManagerImpl::ForceDeviceSyncNow(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const base::Optional<std::string>& session_id) {
  scheduler_->RequestDeviceSync(invocation_reason, session_id);
}

base::Optional<base::Time> CryptAuthV2DeviceManagerImpl::GetLastDeviceSyncTime()
    const {
  return scheduler_->GetLastSuccessfulDeviceSyncTime();
}

base::Optional<base::TimeDelta>
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

  // TODO(nohle): Log invocation reason metric.

  if (!client_app_metadata_) {
    // GCM registration is expected to be completed before the first enrollment.
    DCHECK(!gcm_manager_->GetRegistrationId().empty())
        << "DeviceSync requested before GCM registration complete.";

    SetState(State::kWaitingForClientAppMetadata);
    client_app_metadata_provider_->GetClientAppMetadata(
        gcm_manager_->GetRegistrationId(),
        base::BindOnce(
            &CryptAuthV2DeviceManagerImpl::OnClientAppMetadataFetched,
            callback_weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  AttemptDeviceSync();
}

void CryptAuthV2DeviceManagerImpl::OnResyncMessage(
    const base::Optional<std::string>& session_id,
    const base::Optional<CryptAuthFeatureType>& feature_type) {
  PA_LOG(VERBOSE) << "Received GCM message to re-sync devices (session ID: "
                  << session_id.value_or("[No session ID]") << ").";

  ForceDeviceSyncNow(cryptauthv2::ClientMetadata::SERVER_INITIATED, session_id);
}

void CryptAuthV2DeviceManagerImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state << ".";
  state_ = state;

  // Note: CryptAuthDeviceSyncerImpl guarantees that the callback passed to its
  // public method is always invoked; in other words, the class handles is
  // relevant timeouts internally.
  if (state_ != State::kWaitingForClientAppMetadata)
    return;

  // TODO(https://crbug.com/936273): Add metrics to track failure rates due
  // to async timeouts.
  timer_->Start(FROM_HERE, kWaitingForClientAppMetadataTimeout,
                base::BindOnce(&CryptAuthV2DeviceManagerImpl::OnTimeout,
                               callback_weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthV2DeviceManagerImpl::OnTimeout() {
  DCHECK_EQ(State::kWaitingForClientAppMetadata, state_);

  OnDeviceSyncFinished(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                    kErrorTimeoutWaitingForClientAppMetadata,
                                false /* did_device_registry_change */,
                                base::nullopt /* client_directive */));
}

void CryptAuthV2DeviceManagerImpl::OnClientAppMetadataFetched(
    const base::Optional<cryptauthv2::ClientAppMetadata>& client_app_metadata) {
  DCHECK(state_ == State::kWaitingForClientAppMetadata);

  bool success = client_app_metadata.has_value();

  if (!success) {
    OnDeviceSyncFinished(
        CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                      kErrorClientAppMetadataFetchFailed,
                                  false /* did_device_registry_change */,
                                  base::nullopt /* client_directive */));
    return;
  }

  client_app_metadata_ = client_app_metadata;

  AttemptDeviceSync();
}

void CryptAuthV2DeviceManagerImpl::AttemptDeviceSync() {
  DCHECK(current_client_metadata_);
  DCHECK(client_app_metadata_);

  device_syncer_ = CryptAuthDeviceSyncerImpl::Factory::Get()->BuildInstance(
      device_registry_, key_registry_, client_factory_);

  SetState(State::kWaitingForDeviceSync);

  device_syncer_->Sync(
      *current_client_metadata_, *client_app_metadata_,
      base::BindOnce(&CryptAuthV2DeviceManagerImpl::OnDeviceSyncFinished,
                     callback_weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthV2DeviceManagerImpl::OnDeviceSyncFinished(
    CryptAuthDeviceSyncResult device_sync_result) {
  // Once a DeviceSync attempt finishes, no other callbacks should be invoked.
  // This is particularly relevant for timeout failures.
  callback_weak_ptr_factory_.InvalidateWeakPtrs();

  // The DeviceSync result might be owned by the device syncer, so we copy the
  // result here before destroying the device syncer.
  CryptAuthDeviceSyncResult device_sync_result_copy = device_sync_result;
  device_syncer_.reset();

  std::stringstream prefix;
  prefix << "DeviceSync attempt with invocation reason "
         << current_client_metadata_->invocation_reason();
  std::stringstream suffix;
  suffix << "with result code " << device_sync_result_copy.result_code() << ".";
  switch (device_sync_result_copy.GetResultType()) {
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
               << (device_sync_result_copy.did_device_registry_change()
                       ? "changed."
                       : "did not change.");

  current_client_metadata_.reset();

  // TODO(nohle): Log DeviceSync result metrics: success, result code, and if
  // devices changed.

  scheduler_->HandleDeviceSyncResult(device_sync_result_copy);

  base::Optional<base::TimeDelta> time_to_next_attempt = GetTimeToNextAttempt();
  if (time_to_next_attempt) {
    PA_LOG(INFO) << "Time until next DeviceSync attempt: "
                 << *time_to_next_attempt << ".";
  } else {
    PA_LOG(INFO) << "No future DeviceSync requests currently scheduled.";
  }

  if (!device_sync_result_copy.IsSuccess()) {
    PA_LOG(INFO) << "Number of consecutive DeviceSync failures: "
                 << scheduler_->GetNumConsecutiveDeviceSyncFailures() << ".";
  }

  SetState(State::kIdle);

  NotifyDeviceSyncFinished(device_sync_result_copy);
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthV2DeviceManagerImpl::State& state) {
  switch (state) {
    case CryptAuthV2DeviceManagerImpl::State::kIdle:
      stream << "[DeviceManager state: Idle]";
      break;
    case CryptAuthV2DeviceManagerImpl::State::kWaitingForClientAppMetadata:
      stream << "[DeviceManager state: Waiting for ClientAppMetadata]";
      break;
    case CryptAuthV2DeviceManagerImpl::State::kWaitingForDeviceSync:
      stream << "[DeviceManager state: Waiting for DeviceSync to finish]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
