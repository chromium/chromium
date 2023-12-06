// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_notifier_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"

namespace ash {

namespace device_sync {

namespace {

// TODO(https://crbug.com/933656): Use async execution time metric to tune this.
constexpr base::TimeDelta kWaitingForBatchNotifyGroupDevicesResponseTimeout =
    kMaxAsyncExecutionTime;

void RecordBatchNotifyGroupDevicesMetrics(const base::TimeDelta& execution_time,
                                          CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.DeviceNotifier.ExecutionTime.NotifyGroupDevices",
      execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.DeviceSyncV2.DeviceNotifier.ApiCallResult.NotifyGroupDevices",
      result);
}

}  // namespace

// static
CryptAuthDeviceNotifierImpl::Factory*
    CryptAuthDeviceNotifierImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthDeviceNotifier>
CryptAuthDeviceNotifierImpl::Factory::Create(
    const std::string& instance_id,
    const std::string& instance_id_token,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(instance_id, instance_id_token,
                                         client_factory, std::move(timer));
  }

  return base::WrapUnique(new CryptAuthDeviceNotifierImpl(
      instance_id, instance_id_token, client_factory, std::move(timer)));
}

// static
void CryptAuthDeviceNotifierImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthDeviceNotifierImpl::Factory::~Factory() = default;

CryptAuthDeviceNotifierImpl::CryptAuthDeviceNotifierImpl(
    const std::string& instance_id,
    const std::string& instance_id_token,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : instance_id_(instance_id),
      instance_id_token_(instance_id_token),
      client_factory_(client_factory),
      timer_(std::move(timer)) {}

CryptAuthDeviceNotifierImpl::~CryptAuthDeviceNotifierImpl() = default;

// static
std::optional<base::TimeDelta> CryptAuthDeviceNotifierImpl::GetTimeoutForState(
    State state) {
  switch (state) {
    case State::kWaitingForBatchNotifyGroupDevicesResponse:
      return kWaitingForBatchNotifyGroupDevicesResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return std::nullopt;
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

  std::optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthDeviceNotifierImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthDeviceNotifierImpl::OnTimeout() {
  DCHECK_EQ(state_, State::kWaitingForBatchNotifyGroupDevicesResponse);
  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  RecordBatchNotifyGroupDevicesMetrics(execution_time,
                                       CryptAuthApiCallResult::kTimeout);
  PA_LOG(ERROR) << "Timed out in state " << state_ << ".";

  // TODO(https://crbug.com/1011358): Use more specific error codes.
  FinishAttempt(NetworkRequestError::kUnknown);
}

void CryptAuthDeviceNotifierImpl::ProcessRequestQueue() {
  if (pending_requests_.empty())
    return;

  cryptauthv2::BatchNotifyGroupDevicesRequest request;
  request.mutable_context()->set_group(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether));
  request.mutable_context()->mutable_client_metadata()->set_invocation_reason(
      cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED);
  request.mutable_context()->set_device_id(instance_id_);
  request.mutable_context()->set_device_id_token(instance_id_token_);
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
      base::BindOnce(
          &CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesSuccess,
          base::Unretained(this)),
      base::BindOnce(
          &CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesFailure,
          base::Unretained(this)));
}

void CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesSuccess(
    const cryptauthv2::BatchNotifyGroupDevicesResponse& response) {
  DCHECK_EQ(State::kWaitingForBatchNotifyGroupDevicesResponse, state_);

  RecordBatchNotifyGroupDevicesMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthApiCallResult::kSuccess);

  FinishAttempt(std::nullopt /* error */);
}

void CryptAuthDeviceNotifierImpl::OnBatchNotifyGroupDevicesFailure(
    NetworkRequestError error) {
  DCHECK_EQ(State::kWaitingForBatchNotifyGroupDevicesResponse, state_);

  RecordBatchNotifyGroupDevicesMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthApiCallResultFromNetworkRequestError(error));
  PA_LOG(ERROR) << "BatchNotifyGroupDevices call failed with error " << error
                << ".";

  FinishAttempt(error);
}

void CryptAuthDeviceNotifierImpl::FinishAttempt(
    std::optional<NetworkRequestError> error) {
  cryptauth_client_.reset();
  SetState(State::kIdle);

  DCHECK(!pending_requests_.empty());
  Request current_request = std::move(pending_requests_.front());
  pending_requests_.pop();

  if (error) {
    std::move(current_request.error_callback).Run(*error);
  } else {
    PA_LOG(VERBOSE) << "NotifyDevices attempt succeeded.";
    std::move(current_request.success_callback).Run();
  }

  ProcessRequestQueue();
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthDeviceNotifierImpl::State& state) {
  switch (state) {
    case CryptAuthDeviceNotifierImpl::State::kIdle:
      stream << "[CryptAuthDeviceNotifier state: Idle]";
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

}  // namespace ash
