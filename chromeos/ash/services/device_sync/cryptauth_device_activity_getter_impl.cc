// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_activity_getter_impl.h"

#include <array>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/ash/services/device_sync/device_sync_type_converters.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_logging.h"

namespace ash {

namespace device_sync {

namespace {

// TODO(https://crbug.com/933656): Use async execution time metrics to tune
// these timeout values.
constexpr base::TimeDelta kWaitingForGetDevicesActivityStatusResponseTimeout =
    kMaxAsyncExecutionTime;

void RecordGetDevicesActivityStatusMetrics(
    const base::TimeDelta& execution_time,
    CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.DeviceActivityGetter.ExecutionTime."
      "GetDevicesActivityStatus",
      execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.DeviceSyncV2.DeviceActivityGetter.ApiCallResult."
      "GetDevicesActivityStatus",
      result);
}

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
std::optional<base::TimeDelta>
CryptAuthDeviceActivityGetterImpl::GetTimeoutForState(State state) {
  switch (state) {
    case State::kWaitingForGetDevicesActivityStatusResponse:
      return kWaitingForGetDevicesActivityStatusResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return std::nullopt;
  }
}

CryptAuthDeviceActivityGetterImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthDeviceActivityGetter>
CryptAuthDeviceActivityGetterImpl::Factory::Create(
    const std::string& instance_id,
    const std::string& instance_id_token,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_)
    return test_factory_->CreateInstance(instance_id, instance_id_token,
                                         client_factory, std::move(timer));

  return base::WrapUnique(new CryptAuthDeviceActivityGetterImpl(
      instance_id, instance_id_token, client_factory, std::move(timer)));
}

CryptAuthDeviceActivityGetterImpl::CryptAuthDeviceActivityGetterImpl(
    const std::string& instance_id,
    const std::string& instance_id_token,
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : instance_id_(instance_id),
      instance_id_token_(instance_id_token),
      client_factory_(client_factory),
      timer_(std::move(timer)) {
  DCHECK(client_factory);
}

CryptAuthDeviceActivityGetterImpl::~CryptAuthDeviceActivityGetterImpl() =
    default;

void CryptAuthDeviceActivityGetterImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;
  last_state_change_timestamp_ = base::TimeTicks::Now();

  std::optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthDeviceActivityGetterImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthDeviceActivityGetterImpl::OnAttemptStarted() {
  cryptauthv2::GetDevicesActivityStatusRequest request;
  request.mutable_context()->mutable_client_metadata()->set_retry_count(0);
  request.mutable_context()->mutable_client_metadata()->set_invocation_reason(
      cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED);
  request.mutable_context()->set_group(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether));
  request.mutable_context()->set_device_id(instance_id_);
  request.mutable_context()->set_device_id_token(instance_id_token_);

  SetState(State::kWaitingForGetDevicesActivityStatusResponse);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->GetDevicesActivityStatus(
      request,
      base::BindOnce(
          &CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusSuccess,
          base::Unretained(this)),
      base::BindOnce(
          &CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusFailure,
          base::Unretained(this)));
}

void CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusSuccess(
    const cryptauthv2::GetDevicesActivityStatusResponse& response) {
  DCHECK(state_ == State::kWaitingForGetDevicesActivityStatusResponse);

  RecordGetDevicesActivityStatusMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthApiCallResult::kSuccess);

  PA_LOG(VERBOSE) << "GetDevicesActivityStatus response:\n" << response;

  DeviceActivityStatusResult device_activity_statuses;

  for (const cryptauthv2::DeviceActivityStatus& device_activity_status :
       response.device_activity_statuses()) {
    device_activity_statuses.emplace_back(mojom::DeviceActivityStatus::New(
        device_activity_status.device_id(),
        base::Time::FromTimeT(device_activity_status.last_activity_time_sec()),
        std::move(device_activity_status.connectivity_status()),
        base::Time::FromTimeT(
            device_activity_status.last_update_time().seconds()) +
            base::Nanoseconds(
                device_activity_status.last_update_time().nanos())));
  }

  cryptauth_client_.reset();
  SetState(State::kFinished);
  FinishAttemptSuccessfully(std::move(device_activity_statuses));
}

void CryptAuthDeviceActivityGetterImpl::OnGetDevicesActivityStatusFailure(
    NetworkRequestError error) {
  DCHECK(state_ == State::kWaitingForGetDevicesActivityStatusResponse);

  RecordGetDevicesActivityStatusMetrics(
      base::TimeTicks::Now() - last_state_change_timestamp_,
      CryptAuthApiCallResultFromNetworkRequestError(error));

  OnAttemptError(error);
}

void CryptAuthDeviceActivityGetterImpl::OnTimeout() {
  DCHECK_EQ(state_, State::kWaitingForGetDevicesActivityStatusResponse);
  PA_LOG(ERROR) << "Timed out in state " << state_ << ".";

  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  RecordGetDevicesActivityStatusMetrics(execution_time,
                                        CryptAuthApiCallResult::kTimeout);

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

}  // namespace ash
