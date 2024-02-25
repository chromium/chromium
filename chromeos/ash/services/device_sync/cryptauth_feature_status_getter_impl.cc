// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter_impl.h"

#include <array>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"

namespace ash {

namespace device_sync {

namespace {

// Timeout value for asynchronous operation.
// TODO(https://crbug.com/933656): Use async execution time metric to tune this
// timeout value. For now, set the timeout to the max execution time recorded by
// the metrics.
constexpr base::TimeDelta kWaitingForBatchGetFeatureStatusesResponseTimeout =
    kMaxAsyncExecutionTime;

constexpr std::array<multidevice::SoftwareFeature, 16> kAllSoftwareFeatures = {
    multidevice::SoftwareFeature::kBetterTogetherHost,
    multidevice::SoftwareFeature::kBetterTogetherClient,
    multidevice::SoftwareFeature::kSmartLockHost,
    multidevice::SoftwareFeature::kSmartLockClient,
    multidevice::SoftwareFeature::kInstantTetheringHost,
    multidevice::SoftwareFeature::kInstantTetheringClient,
    multidevice::SoftwareFeature::kMessagesForWebHost,
    multidevice::SoftwareFeature::kMessagesForWebClient,
    multidevice::SoftwareFeature::kPhoneHubHost,
    multidevice::SoftwareFeature::kPhoneHubClient,
    multidevice::SoftwareFeature::kWifiSyncHost,
    multidevice::SoftwareFeature::kWifiSyncClient,
    multidevice::SoftwareFeature::kEcheHost,
    multidevice::SoftwareFeature::kEcheClient,
    multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
    multidevice::SoftwareFeature::kPhoneHubCameraRollClient};

CryptAuthDeviceSyncResult::ResultCode
BatchGetFeatureStatusesNetworkRequestErrorToResultCode(
    NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallUnknownError;
  }
}

CryptAuthFeatureStatusGetter::SoftwareFeatureStateMap
ConvertFeatureStatusesToSoftwareFeatureMap(
    const ::google::protobuf::RepeatedPtrField<
        cryptauthv2::DeviceFeatureStatus::FeatureStatus>& feature_statuses,
    bool* did_non_fatal_error_occur) {
  base::flat_set<multidevice::SoftwareFeature> marked_supported;
  base::flat_set<multidevice::SoftwareFeature> marked_enabled;
  for (const cryptauthv2::DeviceFeatureStatus::FeatureStatus& status :
       feature_statuses) {
    std::optional<CryptAuthFeatureType> feature_type =
        CryptAuthFeatureTypeFromString(status.feature_type());

    bool is_known_feature_type = feature_type.has_value();
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.FeatureStatusGetter.IsKnownFeatureType",
        is_known_feature_type);
    if (!is_known_feature_type) {
      PA_LOG(ERROR) << "Unknown feature type: " << status.feature_type();
      *did_non_fatal_error_occur = true;
      continue;
    }

    if (base::Contains(GetSupportedCryptAuthFeatureTypes(), *feature_type) &&
        status.enabled()) {
      marked_supported.insert(
          CryptAuthFeatureTypeToSoftwareFeature(*feature_type));
      continue;
    }

    if (base::Contains(GetEnabledCryptAuthFeatureTypes(), *feature_type) &&
        status.enabled()) {
      marked_enabled.insert(
          CryptAuthFeatureTypeToSoftwareFeature(*feature_type));
      continue;
    }
  }

  CryptAuthFeatureStatusGetter::SoftwareFeatureStateMap feature_states;
  for (const multidevice::SoftwareFeature& feature : kAllSoftwareFeatures) {
    bool is_marked_supported = base::Contains(marked_supported, feature);
    bool is_marked_enabled = base::Contains(marked_enabled, feature);
    bool is_unsupported_feature_marked_enabled =
        !is_marked_supported && is_marked_enabled;

    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.FeatureStatusGetter."
        "IsUnsupportedFeatureMarkedEnabled",
        is_unsupported_feature_marked_enabled);
    if (is_unsupported_feature_marked_enabled) {
      PA_LOG(ERROR) << "SoftwareFeature " << feature << " flagged as enabled "
                    << "but not supported. Marking unsupported.";
      *did_non_fatal_error_occur = true;
    }

    feature_states[feature] =
        is_marked_supported
            ? (is_marked_enabled
                   ? multidevice::SoftwareFeatureState::kEnabled
                   : multidevice::SoftwareFeatureState::kSupported)
            : multidevice::SoftwareFeatureState::kNotSupported;
  }

  return feature_states;
}

base::Time GetMaxLastModifiedTimeFromFeatureStatuses(
    const ::google::protobuf::RepeatedPtrField<
        cryptauthv2::DeviceFeatureStatus::FeatureStatus>& feature_statuses) {
  int64_t max_last_modified_time_millis = 0;
  for (const cryptauthv2::DeviceFeatureStatus::FeatureStatus& status :
       feature_statuses) {
    max_last_modified_time_millis = std::max(status.last_modified_time_millis(),
                                             max_last_modified_time_millis);
  }

  return base::Time::FromMillisecondsSinceUnixEpoch(
      max_last_modified_time_millis);
}

void RecordGetFeatureStatusesMetrics(base::TimeDelta execution_time,
                                     CryptAuthApiCallResult result) {
  LogAsyncExecutionTimeMetric(
      "CryptAuth.DeviceSyncV2.FeatureStatusGetter.ExecutionTime."
      "GetFeatureStatuses",
      execution_time);
  LogCryptAuthApiCallSuccessMetric(
      "CryptAuth.DeviceSyncV2.FeatureStatusGetter.ApiCallResult."
      "GetFeatureStatuses",
      result);
}

}  // namespace

// static
CryptAuthFeatureStatusGetterImpl::Factory*
    CryptAuthFeatureStatusGetterImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthFeatureStatusGetter>
CryptAuthFeatureStatusGetterImpl::Factory::Create(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (test_factory_)
    return test_factory_->CreateInstance(client_factory, std::move(timer));

  return base::WrapUnique(
      new CryptAuthFeatureStatusGetterImpl(client_factory, std::move(timer)));
}

// static
void CryptAuthFeatureStatusGetterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthFeatureStatusGetterImpl::Factory::~Factory() = default;

CryptAuthFeatureStatusGetterImpl::CryptAuthFeatureStatusGetterImpl(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_factory_(client_factory), timer_(std::move(timer)) {
  DCHECK(client_factory);
}

CryptAuthFeatureStatusGetterImpl::~CryptAuthFeatureStatusGetterImpl() = default;

void CryptAuthFeatureStatusGetterImpl::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const base::flat_set<std::string>& device_ids) {
  cryptauthv2::BatchGetFeatureStatusesRequest request;
  request.mutable_context()->CopyFrom(request_context);
  *request.mutable_device_ids() = {device_ids.begin(), device_ids.end()};
  *request.mutable_feature_types() = {
      GetAllCryptAuthFeatureTypeStrings().begin(),
      GetAllCryptAuthFeatureTypeStrings().end()};

  start_get_feature_statuses_timestamp_ = base::TimeTicks::Now();

  timer_->Start(
      FROM_HERE, kWaitingForBatchGetFeatureStatusesResponseTimeout,
      base::BindOnce(
          &CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesTimeout,
          base::Unretained(this)));

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->BatchGetFeatureStatuses(
      request,
      base::BindOnce(
          &CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesSuccess,
          base::Unretained(this), device_ids),
      base::BindOnce(
          &CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesFailure,
          base::Unretained(this)));
}

void CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesSuccess(
    const base::flat_set<std::string>& input_device_ids,
    const cryptauthv2::BatchGetFeatureStatusesResponse& feature_response) {
  DCHECK(id_to_device_software_feature_info_map_.empty());

  RecordGetFeatureStatusesMetrics(
      base::TimeTicks::Now() - start_get_feature_statuses_timestamp_,
      CryptAuthApiCallResult::kSuccess);

  bool did_non_fatal_error_occur = false;
  for (const cryptauthv2::DeviceFeatureStatus& device_feature_status :
       feature_response.device_feature_statuses()) {
    const std::string& id = device_feature_status.device_id();

    bool was_id_requested = base::Contains(input_device_ids, id);
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.FeatureStatusGetter."
        "WasDeviceInResponseRequested",
        was_id_requested);
    if (!was_id_requested) {
      PA_LOG(ERROR) << "Unrequested device (ID: " << id
                    << ") in BatchGetFeatureStatuses response.";
      did_non_fatal_error_occur = true;
      continue;
    }

    bool is_duplicate_id =
        base::Contains(id_to_device_software_feature_info_map_, id);
    base::UmaHistogramBoolean(
        "CryptAuth.DeviceSyncV2.FeatureStatusGetter.IsDuplicateDeviceId",
        is_duplicate_id);
    if (is_duplicate_id) {
      PA_LOG(ERROR) << "Duplicate device IDs (" << id
                    << ") in BatchGetFeatureStatuses response.";
      did_non_fatal_error_occur = true;
      continue;
    }

    id_to_device_software_feature_info_map_.try_emplace(
        device_feature_status.device_id(),
        ConvertFeatureStatusesToSoftwareFeatureMap(
            device_feature_status.feature_statuses(),
            &did_non_fatal_error_occur),
        GetMaxLastModifiedTimeFromFeatureStatuses(
            device_feature_status.feature_statuses()));
  }

  bool correct_number_of_devices_in_response =
      input_device_ids.size() == id_to_device_software_feature_info_map_.size();
  base::UmaHistogramBoolean(
      "CryptAuth.DeviceSyncV2.FeatureStatusGetter."
      "CorrectNumberOfDevicesInResponse",
      correct_number_of_devices_in_response);
  if (!correct_number_of_devices_in_response) {
    PA_LOG(ERROR) << "Incorrect number of devices in BatchGetFeatureStatuses "
                  << "response. Expected: " << input_device_ids.size()
                  << ". Received: "
                  << id_to_device_software_feature_info_map_.size() << ".";
    did_non_fatal_error_occur = true;
  }

  CryptAuthDeviceSyncResult::ResultCode result_code =
      did_non_fatal_error_occur
          ? CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors
          : CryptAuthDeviceSyncResult::ResultCode::kSuccess;
  FinishAttempt(result_code);
}

void CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesFailure(
    NetworkRequestError error) {
  RecordGetFeatureStatusesMetrics(
      base::TimeTicks::Now() - start_get_feature_statuses_timestamp_,
      CryptAuthApiCallResultFromNetworkRequestError(error));

  FinishAttempt(BatchGetFeatureStatusesNetworkRequestErrorToResultCode(error));
}

void CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesTimeout() {
  RecordGetFeatureStatusesMetrics(
      base::TimeTicks::Now() - start_get_feature_statuses_timestamp_,
      CryptAuthApiCallResult::kTimeout);

  FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                    kErrorTimeoutWaitingForBatchGetFeatureStatusesResponse);
}

void CryptAuthFeatureStatusGetterImpl::FinishAttempt(
    CryptAuthDeviceSyncResult::ResultCode result_code) {
  cryptauth_client_.reset();
  timer_->Stop();

  OnAttemptFinished(id_to_device_software_feature_info_map_, result_code);
}

}  // namespace device_sync

}  // namespace ash
