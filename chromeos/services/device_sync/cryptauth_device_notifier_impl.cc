// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_notifier_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/public/cpp/client_app_metadata_provider.h"

namespace chromeos {

namespace device_sync {

namespace {

// Timeout values for asynchronous operation.
// TODO(https://crbug.com/933656): Use async execution time metric to tune these
// timeout values.
constexpr base::TimeDelta kWaitingForClientAppMetadataTimeout =
    base::TimeDelta::FromSeconds(60);
constexpr base::TimeDelta kWaitingForBatchNotifyGroupDevicesResponseTimeout =
    kMaxAsyncExecutionTime;

void RecordClientAppMetadataFetchMetrics(const base::TimeDelta& execution_time,
                                         CryptAuthAsyncTaskResult result) {
  // TODO(https://crbug.com/933656, https://crbug.com/936273): Add metrics to
  // track async execution times and failure rates due to async timeouts.
}

void RecordBatchNotifyGroupDevicesMetrics(const base::TimeDelta& execution_time,
                                          CryptAuthApiCallResult result) {
  // TODO(https://crbug.com/933656, https://crbug.com/936273): Add metrics to
  // track async execution times and failure rates due to async timeouts.
}

}  // namespace

// static
CryptAuthDeviceNotifierImpl::Factory*
    CryptAuthDeviceNotifierImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthDeviceNotifierImpl::Factory*
CryptAuthDeviceNotifierImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthDeviceNotifierImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthDeviceNotifierImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthDeviceNotifierImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthDeviceNotifier>
CryptAuthDeviceNotifierImpl::Factory::BuildInstance(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new CryptAuthDeviceNotifierImpl(
      client_app_metadata_provider, client_factory, gcm_manager,
      std::move(timer)));
}

CryptAuthDeviceNotifierImpl::CryptAuthDeviceNotifierImpl(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_app_metadata_provider_(client_app_metadata_provider),
      client_factory_(client_factory),
      gcm_manager_(gcm_manager),
      timer_(std::move(timer)) {}

CryptAuthDeviceNotifierImpl::~CryptAuthDeviceNotifierImpl() = default;

// static
base::Optional<base::TimeDelta> CryptAuthDeviceNotifierImpl::GetTimeoutForState(
    State state) {
  switch (state) {
    case State::kWaitingForClientAppMetadata:
      return kWaitingForClientAppMetadataTimeout;
    case State::kWaitingForBatchNotifyGroupDevicesResponse:
      return kWaitingForBatchNotifyGroupDevicesResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return base::nullopt;
  }
}

CryptAuthDeviceNotifierImpl::Request::Request(
    const base::flat_set<std::string>& device_ids,
    cryptauthv2::TargetService target_service,
    CryptAuthFeatureType feature_type,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback)
    : device_ids(device_ids),
      target_service(target_service),
      feature_type(feature_type),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

CryptAuthDeviceNotifierImpl::Request::Request(Request&& request)
    : device_ids(std::move(request.device_ids)),
      target_service(request.target_service),
      feature_type(request.feature_type),
      success_callback(std::move(request.success_callback)),
      error_callback(std::move(request.error_callback)) {}

CryptAuthDeviceNotifierImpl::Request::~Request() = default;

void CryptAuthDeviceNotifierImpl::NotifyDevices(
    const base::flat_set<std::string>& device_ids,
    cryptauthv2::TargetService target_service,
    CryptAuthFeatureType feature_type,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback) {
  DCHECK(!device_ids.empty());
  DCHECK_NE(cryptauthv2::TargetService::TARGET_SERVICE_UNSPECIFIED,
            target_service);

  pending_requests_.emplace(device_ids, target_service, feature_type,
                            std::move(success_callback),
                            std::move(error_callback));

  if (state_ == State::kIdle)
    ProcessRequestQueue();
}

void CryptAuthDeviceNotifierImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;
  last_state_change_timestamp_ = base::TimeTicks::Now();

  base::Optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthDeviceNotifierImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthDeviceNotifierImpl::OnTimeout() {
  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  switch (state_) {
    case State::kWaitingForClientAppMetadata:
      RecordClientAppMetadataFetchMetrics(execution_time,
                                          CryptAuthAsyncTaskResult::kTimeout);
      break;
    case State::kWaitingForBatchNotifyGroupDevicesResponse:
      RecordBatchNotifyGroupDevicesMetrics(execution_time,
                                           CryptAuthApiCallResult::kTimeout);
      break;
    default:
      NOTREACHED();
  }

  PA_LOG(ERROR) << "Timed out in state " << state_ << ".";

  // TODO(https://crbug.com/1011358): Use more specific error codes.
  FinishAttempt(NetworkRequestError::kUnknown);
}

void CryptAuthDeviceNotifierImpl::ProcessRequestQueue() {
  if (pending_requests_.empty())
    return;

  if (!client_app_metadata_) {
    // GCM registration is expected to be completed before the first enrollment.
    DCHECK(!gcm_manager_->GetRegistrationId().empty())
        << "DeviceSync requested before GCM registration complete.";

    SetState(State::kWaitingForClientAppMetadata);
    client_app_metadata_provider_->GetClientAppMetadata(
        gcm_manager_->GetRegistrationId(),
        base::BindOnce(&CryptAuthDeviceNotifierImpl::OnClientAppMetadataFetched,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  cryptauthv2::BatchNotifyGroupDevicesRequest request;
  request.mutable_context()->set_group(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether));
  request.mutable_context()->mutable_client_metadata()->set_invocation_reason(
      cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED);
  request.mutable_context()->set_device_id(client_app_metadata_->instance_id());
  request.mutable_context()->set_device_id_token(
      client_app_metadata_->instance_id_token());
  *request.mutable_notify_device_ids() = {
      pending_requests_.front().device_ids.begin(),
      pending_requests_.front().device_ids.end()};
  request.set_target_service(pending_requests_.front().target_service);
  request.set_feature_type(
      CryptAuthFeatureTypeToString(pending_requests_.front().feature_type));

  SetState(State::kWaitingForBatchNotifyGroupDevicesResponse);
  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->BatchNotifyGroupDevices(
      request,
      base::Bind(&CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesSuccess,
                 base::Unretained(this)),
      base::Bind(&CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesFailure,
                 base::Unretained(this)));
}

void CryptAuthDeviceNotifierImpl::OnClientAppMetadataFetched(
    const base::Optional<cryptauthv2::ClientAppMetadata>& client_app_metadata) {
  DCHECK_EQ(State::kWaitingForClientAppMetadata, state_);

  bool success = client_app_metadata.has_value();

  RecordClientAppMetadataFetchMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      success ? CryptAuthAsyncTaskResult::kSuccess
              : CryptAuthAsyncTaskResult::kError);

  if (!success) {
    PA_LOG(ERROR) << "ClientAppMetadata fetch failed.";

    // TODO(https://crbug.com/1011358): Use more specific error codes.
    FinishAttempt(NetworkRequestError::kUnknown);

    return;
  }

  client_app_metadata_ = client_app_metadata;
  ProcessRequestQueue();
}

void CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesSuccess(
    const cryptauthv2::BatchNotifyGroupDevicesResponse& response) {
  DCHECK_EQ(State::kWaitingForBatchNotifyGroupDevicesResponse, state_);
  FinishAttempt(base::nullopt /* error */);
}

void CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesFailure(
    NetworkRequestError error) {
  DCHECK_EQ(State::kWaitingForBatchNotifyGroupDevicesResponse, state_);
  PA_LOG(ERROR) << "BatchNotifyGroupDevices call failed with error " << error
                << ".";
  FinishAttempt(error);
}

void CryptAuthDeviceNotifierImpl::FinishAttempt(
    base::Optional<NetworkRequestError> error) {
  DCHECK(!pending_requests_.empty());

  Request current_request = std::move(pending_requests_.front());
  pending_requests_.pop();

  if (error) {
    std::move(current_request.error_callback).Run(*error);
  } else {
    PA_LOG(VERBOSE) << "NotifyDevices attempt succeeded.";
    std::move(current_request.success_callback).Run();
  }

  SetState(State::kIdle);
  ProcessRequestQueue();
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthDeviceNotifierImpl::State& state) {
  switch (state) {
    case CryptAuthDeviceNotifierImpl::State::kIdle:
      stream << "[CryptAuthDeviceNotifier state: Idle]";
      break;
    case CryptAuthDeviceNotifierImpl::State::kWaitingForClientAppMetadata:
      stream << "[CryptAuthDeviceNotifier state: Waiting for "
             << "ClientAppMetadata]";
      break;
    case CryptAuthDeviceNotifierImpl::State::
        kWaitingForBatchNotifyGroupDevicesResponse:
      stream << "[CryptAuthDeviceNotifier state: Waiting for "
             << "BatchNotifyGroupDevices response]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
