// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_activity_getter_impl.h"

#include <array>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/device_sync_type_converters.h"

namespace chromeos {

namespace device_sync {

namespace {

// Timeout values for the asynchronous operations of fetching client app
// metadata and making the network request.
// TODO(https://crbug.com/933656): Tune this value.
constexpr base::TimeDelta kWaitingForClientAppMetadataTimeout =
    base::TimeDelta::FromSeconds(60);
constexpr base::TimeDelta kWaitingForGetDevicesActivityStatusResponseTimeout =
    base::TimeDelta::FromSeconds(10);

}  // namespace

// static
CryptAuthDeviceActivityGetterImpl::Factory*
    CryptAuthDeviceActivityGetterImpl::Factory::test_factory_ = nullptr;

// static
void CryptAuthDeviceActivityGetterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

// static
base::Optional<base::TimeDelta>
CryptAuthDeviceActivityGetterImpl::GetTimeoutForState(State state) {
  switch (state) {
    case State::kWaitingForClientAppMetadata:
      return kWaitingForClientAppMetadataTimeout;
    case State::kWaitingForGetDevicesActivityStatusResponse:
      return kWaitingForGetDevicesActivityStatusResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return base::nullopt;
  }
}

CryptAuthDeviceActivityGetterImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthDeviceActivityGetter>
CryptAuthDeviceActivityGetterImpl::Factory::Create(
    CryptAuthClientFactory* client_factory,
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthGCMManager* gcm_manager,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_)
    return test_factory_->BuildInstance(client_factory,
                                        client_app_metadata_provider,
                                        gcm_manager, std::move(timer));

  return base::WrapUnique(new CryptAuthDeviceActivityGetterImpl(
      client_factory, client_app_metadata_provider, gcm_manager,
      std::move(timer)));
}

CryptAuthDeviceActivityGetterImpl::CryptAuthDeviceActivityGetterImpl(
    CryptAuthClientFactory* client_factory,
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthGCMManager* gcm_manager,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_factory_(client_factory),
      client_app_metadata_provider_(client_app_metadata_provider),
      gcm_manager_(gcm_manager),
      timer_(std::move(timer)) {
  DCHECK(client_factory);
}

CryptAuthDeviceActivityGetterImpl::~CryptAuthDeviceActivityGetterImpl() =
    default;

void CryptAuthDeviceActivityGetterImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;

  base::Optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  // TODO(https://crbug.com/936273): Add metrics to track failure rates due to
  // async timeouts.
  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthDeviceActivityGetterImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthDeviceActivityGetterImpl::OnAttemptStarted() {
  // GCM registration is expected to be completed before the first enrollment.
  DCHECK(!gcm_manager_->GetRegistrationId().empty())
      << "Device activity status requested before GCM registration complete.";
  SetState(State::kWaitingForClientAppMetadata);

  client_app_metadata_provider_->GetClientAppMetadata(
      gcm_manager_->GetRegistrationId(),
      base::BindOnce(
          &CryptAuthDeviceActivityGetterImpl::OnClientAppMetadataFetched,
          callback_weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthDeviceActivityGetterImpl::OnClientAppMetadataFetched(
    const base::Optional<cryptauthv2::ClientAppMetadata>& client_app_metadata) {
  DCHECK(state_ == State::kWaitingForClientAppMetadata);

  if (!client_app_metadata) {
    OnAttemptError(NetworkRequestError::kUnknown);
    return;
  }

  cryptauthv2::GetDevicesActivityStatusRequest request;

  request.mutable_context()->mutable_client_metadata()->set_retry_count(0);
  request.mutable_context()->mutable_client_metadata()->set_invocation_reason(
      cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED);

  request.mutable_context()->set_group(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether));
  request.mutable_context()->set_device_id(
      client_app_metadata.value().instance_id());
  request.mutable_context()->set_device_id_token(
      client_app_metadata.value().instance_id_token());

  SetState(State::kWaitingForGetDevicesActivityStatusResponse);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->GetDevicesActivityStatus(
      request,
      base::Bind(
          &CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusSuccess,
          base::Unretained(this)),
      base::Bind(
          &CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusFailure,
          base::Unretained(this)));
}

void CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusSuccess(
    const cryptauthv2::GetDevicesActivityStatusResponse& response) {
  DCHECK(state_ == State::kWaitingForGetDevicesActivityStatusResponse);

  DeviceActivityStatusResult device_activity_statuses;

  for (const cryptauthv2::DeviceActivityStatus& device_activity_status :
       response.device_activity_statuses()) {
    device_activity_statuses.emplace_back(mojom::DeviceActivityStatus::New(
        device_activity_status.device_id(),
        base::Time::FromTimeT(device_activity_status.last_activity_time_sec()),
        std::move(device_activity_status.connectivity_status())));
  }

  cryptauth_client_.reset();
  SetState(State::kFinished);
  FinishAttemptSuccessfully(std::move(device_activity_statuses));
}

void CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusFailure(
    NetworkRequestError error) {
  DCHECK(state_ == State::kWaitingForGetDevicesActivityStatusResponse);
  OnAttemptError(error);
}

void CryptAuthDeviceActivityGetterImpl::OnTimeout() {
  OnAttemptError(NetworkRequestError::kUnknown);
}

void CryptAuthDeviceActivityGetterImpl::OnAttemptError(
    NetworkRequestError error) {
  cryptauth_client_.reset();
  SetState(State::kFinished);
  FinishAttemptWithError(error);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthDeviceActivityGetterImpl::State& state) {
  switch (state) {
    case CryptAuthDeviceActivityGetterImpl::State::kNotStarted:
      stream << "[DeviceActivityGetter state: Not started]";
      break;
    case CryptAuthDeviceActivityGetterImpl::State::kWaitingForClientAppMetadata:
      stream << "[DeviceActivityGetter state: Waiting for client app metadata]";
      break;
    case CryptAuthDeviceActivityGetterImpl::State::
        kWaitingForGetDevicesActivityStatusResponse:
      stream << "[DeviceActivityGetter state: Waiting for "
             << "GetDevicesActivityStatus response]";
      break;
    case CryptAuthDeviceActivityGetterImpl::State::kFinished:
      stream << "[DeviceActivityGetter state: Finished]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
