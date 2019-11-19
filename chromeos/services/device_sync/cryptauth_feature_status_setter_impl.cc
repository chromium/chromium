// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_status_setter_impl.h"

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
constexpr base::TimeDelta kWaitingForBatchSetFeatureStatusesResponseTimeout =
    kMaxAsyncExecutionTime;

void RecordClientAppMetadataFetchMetrics(const base::TimeDelta& execution_time,
                                         CryptAuthAsyncTaskResult result) {
  // TODO(https://crbug.com/933656, https://crbug.com/936273): Add metrics to
  // track async execution times and failure rates due to async timeouts.
}

void RecordBatchSetFeatureStatusesMetrics(const base::TimeDelta& execution_time,
                                          CryptAuthApiCallResult result) {
  // TODO(https://crbug.com/933656, https://crbug.com/936273): Add metrics to
  // track async execution times and failure rates due to async timeouts.
}

}  // namespace

// static
CryptAuthFeatureStatusSetterImpl::Factory*
    CryptAuthFeatureStatusSetterImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthFeatureStatusSetterImpl::Factory*
CryptAuthFeatureStatusSetterImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthFeatureStatusSetterImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthFeatureStatusSetterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthFeatureStatusSetterImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthFeatureStatusSetter>
CryptAuthFeatureStatusSetterImpl::Factory::BuildInstance(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new CryptAuthFeatureStatusSetterImpl(
      client_app_metadata_provider, client_factory, gcm_manager,
      std::move(timer)));
}

CryptAuthFeatureStatusSetterImpl::CryptAuthFeatureStatusSetterImpl(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_app_metadata_provider_(client_app_metadata_provider),
      client_factory_(client_factory),
      gcm_manager_(gcm_manager),
      timer_(std::move(timer)) {}

CryptAuthFeatureStatusSetterImpl::~CryptAuthFeatureStatusSetterImpl() = default;

// static
base::Optional<base::TimeDelta>
CryptAuthFeatureStatusSetterImpl::GetTimeoutForState(State state) {
  switch (state) {
    case State::kWaitingForClientAppMetadata:
      return kWaitingForClientAppMetadataTimeout;
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
  base::TimeDelta execution_time =
      base::TimeTicks::Now() - last_state_change_timestamp_;
  switch (state_) {
    case State::kWaitingForClientAppMetadata:
      RecordClientAppMetadataFetchMetrics(execution_time,
                                          CryptAuthAsyncTaskResult::kTimeout);
      break;
    case State::kWaitingForBatchSetFeatureStatusesResponse:
      RecordBatchSetFeatureStatusesMetrics(execution_time,
                                           CryptAuthApiCallResult::kTimeout);
      break;
    default:
      NOTREACHED();
  }

  PA_LOG(ERROR) << "Timed out in state " << state_ << ".";

  // TODO(https://crbug.com/1011358): Use more specific error codes.
  FinishAttempt(NetworkRequestError::kUnknown);
}

void CryptAuthFeatureStatusSetterImpl::ProcessRequestQueue() {
  if (pending_requests_.empty())
    return;

  if (!client_app_metadata_) {
    // GCM registration is expected to be completed before the first enrollment.
    DCHECK(!gcm_manager_->GetRegistrationId().empty())
        << "DeviceSync requested before GCM registration complete.";

    SetState(State::kWaitingForClientAppMetadata);
    client_app_metadata_provider_->GetClientAppMetadata(
        gcm_manager_->GetRegistrationId(),
        base::BindOnce(
            &CryptAuthFeatureStatusSetterImpl::OnClientAppMetadataFetched,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  cryptauthv2::BatchSetFeatureStatusesRequest request;
  request.mutable_context()->set_group(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(
          CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether));
  request.mutable_context()->mutable_client_metadata()->set_invocation_reason(
      cryptauthv2::ClientMetadata::FEATURE_TOGGLED);
  request.mutable_context()->set_device_id(client_app_metadata_->instance_id());
  request.mutable_context()->set_device_id_token(
      client_app_metadata_->instance_id_token());

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
      base::Bind(
          &CryptAuthFeatureStatusSetterImpl::OnBatchSetFeatureStatusesSuccess,
          base::Unretained(this)),
      base::Bind(
          &CryptAuthFeatureStatusSetterImpl::OnBatchSetFeatureStatusesFailure,
          base::Unretained(this)));
}

void CryptAuthFeatureStatusSetterImpl::OnClientAppMetadataFetched(
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
    case CryptAuthFeatureStatusSetterImpl::State::kWaitingForClientAppMetadata:
      stream << "[CryptAuthFeatureStatusSetter state: Waiting for "
             << "ClientAppMetadata]";
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
