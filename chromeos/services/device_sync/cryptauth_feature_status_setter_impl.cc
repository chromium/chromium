// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_status_setter_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

namespace {

// TODO(https://crbug.com/933656): Use async execution time metric to tune this.
constexpr base::TimeDelta kWaitingForBatchSetFeatureStatusesResponseTimeout =
    kMaxAsyncExecutionTime;

void RecordBatchSetFeatureStatusesMetrics(const base::TimeDelta& execution_time,
                                          CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.FeatureStatusSetter.ExecutionTime."
      "SetFeatureStatuses",
      execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.DeviceSyncV2.FeatureStatusSetter.ApiCallResult."
      "SetFeatureStatuses",
      result);
}

}  // namespace

// static
CryptAuthFeatureStatusSetterImpl::Factory*
    CryptAuthFeatureStatusSetterImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthFeatureStatusSetter>
CryptAuthFeatureStatusSetterImpl::Factory::Create(
    const std::string& instance_id,
    const std::string& instance_id_token,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_)
    return test_factory_->CreateInstance(instance_id, instance_id_token,
                                         client_factory, std::move(timer));

  return base::WrapUnique(new CryptAuthFeatureStatusSetterImpl(
      instance_id, instance_id_token, client_factory, std::move(timer)));
}

// static
void CryptAuthFeatureStatusSetterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthFeatureStatusSetterImpl::Factory::~Factory() = default;

CryptAuthFeatureStatusSetterImpl::CryptAuthFeatureStatusSetterImpl(
    const std::string& instance_id,
    const std::string& instance_id_token,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : instance_id_(instance_id),
      instance_id_token_(instance_id_token),
      client_factory_(client_factory),
      timer_(std::move(timer)) {}

CryptAuthFeatureStatusSetterImpl::~CryptAuthFeatureStatusSetterImpl() = default;

// static
base::Optional<base::TimeDelta>
CryptAuthFeatureStatusSetterImpl::GetTimeoutForState(State state) {
  switch (state) {
    case State::kWaitingForBatchSetFeatureStatusesResponse:
      return kWaitingForBatchSetFeatureStatusesResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return base::nullopt;
  }
}

CryptAuthFeatureStatusSetterImpl::Request::Request(
    const std::string& device_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback)
    : device_id(device_id),
      feature(feature),
      status_change(status_change),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

CryptAuthFeatureStatusSetterImpl::Request::Request(Request&& request)
    : device_id(request.device_id),
      feature(request.feature),
      status_change(request.status_change),
      success_callback(std::move(request.success_callback)),
      error_callback(std::move(request.error_callback)) {}

CryptAuthFeatureStatusSetterImpl::Request::~Request() = default;

void CryptAuthFeatureStatusSetterImpl::SetFeatureStatus(
    const std::string& device_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    base::OnceClosure success_callback,
    base::OnceCallback<void(NetworkRequestError)> error_callback) {
  pending_requests_.emplace(device_id, feature, status_change,
                            std::move(success_callback),
                            std::move(error_callback));

  if (state_ == State::kIdle)
    ProcessRequestQueue();
}

void CryptAuthFeatureStatusSetterImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;
  last_state_change_timestamp_ = base::TimeTicks::Now();

  base::Optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthFeatureStatusSetterImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthFeatureStatusSetterImpl::OnTimeout() {
  DCHECK_EQ(state_, State::kWaitingForBatchSetFeatureStatusesResponse);
  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  RecordBatchSetFeatureStatusesMetrics(execution_time,
                                       CryptAuthApiCallResult::kTimeout);
  PA_LOG(ERROR) << "Timed out in state " << state_ << ".";

  // TODO(https://crbug.com/1011358): Use more specific error codes.
  FinishAttempt(NetworkRequestError::kUnknown);
}

void CryptAuthFeatureStatusSetterImpl::ProcessRequestQueue() {
  if (pending_requests_.empty())
    return;

  cryptauthv2::BatchSetFeatureStatusesRequest request;
  request.mutable_context()->set_group(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether));
  request.mutable_context()->mutable_client_metadata()->set_invocation_reason(
      cryptauthv2::ClientMetadata::FEATURE_TOGGLED);
  request.mutable_context()->set_device_id(instance_id_);
  request.mutable_context()->set_device_id_token(instance_id_token_);

  cryptauthv2::DeviceFeatureStatus* device_feature_status =
      request.add_device_feature_statuses();
  device_feature_status->set_device_id(pending_requests_.front().device_id);

  cryptauthv2::DeviceFeatureStatus::FeatureStatus* feature_status =
      device_feature_status->add_feature_statuses();
  feature_status->set_feature_type(
      CryptAuthFeatureTypeToString(CryptAuthFeatureTypeFromSoftwareFeature(
          pending_requests_.front().feature)));

  switch (pending_requests_.front().status_change) {
    case FeatureStatusChange::kEnableExclusively:
      feature_status->set_enabled(true);
      feature_status->set_enable_exclusively(true);
      break;
    case FeatureStatusChange::kEnableNonExclusively:
      feature_status->set_enabled(true);
      feature_status->set_enable_exclusively(false);
      break;
    case FeatureStatusChange::kDisable:
      feature_status->set_enabled(false);
      feature_status->set_enable_exclusively(false);
      break;
  }

  SetState(State::kWaitingForBatchSetFeatureStatusesResponse);
  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->BatchSetFeatureStatuses(
      request,
      base::BindOnce(
          &CryptAuthFeatureStatusSetterImpl::OnBatchSetFeatureStatusesSuccess,
          base::Unretained(this)),
      base::BindOnce(
          &CryptAuthFeatureStatusSetterImpl::OnBatchSetFeatureStatusesFailure,
          base::Unretained(this)));
}

void CryptAuthFeatureStatusSetterImpl::OnBatchSetFeatureStatusesSuccess(
    const cryptauthv2::BatchSetFeatureStatusesResponse& response) {
  DCHECK_EQ(State::kWaitingForBatchSetFeatureStatusesResponse, state_);
  FinishAttempt(base::nullopt /* error */);
}

void CryptAuthFeatureStatusSetterImpl::OnBatchSetFeatureStatusesFailure(
    NetworkRequestError error) {
  DCHECK_EQ(State::kWaitingForBatchSetFeatureStatusesResponse, state_);
  PA_LOG(ERROR) << "BatchSetFeatureStatuses call failed with error " << error
                << ".";
  FinishAttempt(error);
}

void CryptAuthFeatureStatusSetterImpl::FinishAttempt(
    base::Optional<NetworkRequestError> error) {
  DCHECK(!pending_requests_.empty());

  Request current_request = std::move(pending_requests_.front());
  pending_requests_.pop();

  if (error) {
    std::move(current_request.error_callback).Run(*error);
  } else {
    PA_LOG(VERBOSE) << "SetFeatureStatus attempt succeeded.";
    std::move(current_request.success_callback).Run();
  }

  SetState(State::kIdle);
  ProcessRequestQueue();
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthFeatureStatusSetterImpl::State& state) {
  switch (state) {
    case CryptAuthFeatureStatusSetterImpl::State::kIdle:
      stream << "[CryptAuthFeatureStatusSetter state: Idle]";
      break;
    case CryptAuthFeatureStatusSetterImpl::State::
        kWaitingForBatchSetFeatureStatusesResponse:
      stream << "[CryptAuthFeatureStatusSetter state: Waiting for "
             << "BatchSetFeatureStatuses response]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
