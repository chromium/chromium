// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/device_sync_impl.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_client_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_activity_getter_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_notifier_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_enroller_factory_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_setter_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"
#include "chromeos/ash/services/device_sync/device_sync_type_converters.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/proto/device_classifier_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/gcm_device_info_provider.h"
#include "chromeos/ash/services/device_sync/remote_device_provider_impl.h"
#include "chromeos/ash/services/device_sync/software_feature_manager_impl.h"
#include "chromeos/ash/services/device_sync/synced_bluetooth_address_tracker_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace device_sync {

namespace {

constexpr base::TimeDelta kSetFeatureEnabledTimeout = base::Seconds(5);

// Timeout value for asynchronous operation.
// TODO(https://crbug.com/933656): Use async execution time metrics to tune
// this timeout value.
constexpr base::TimeDelta kWaitingForClientAppMetadataTimeout =
    base::Seconds(60);

constexpr base::TimeDelta kBaseRetryDelay = base::Seconds(5);

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other). Entries should be never modified
// or deleted. Only additions possible.
enum class DeviceSyncRequestFailureReason {
  kRequestSucceededButUnexpectedResult = 0,
  kServiceNotYetInitialized = 1,
  kOffline = 2,
  kEndpointNotFound = 3,
  kAuthenticationError = 4,
  kBadRequest = 5,
  kResponseMalformed = 6,
  kInternalServerError = 7,
  kUnknownNetworkError = 8,
  kUnknown = 9,
  kMaxValue = kUnknown
};

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other). Entries should be never modified
// or deleted. Only additions possible.
enum class ForceCryptAuthOperationResult {
  kSuccess = 0,
  kServiceNotReady = 1,
  kMaxValue = kServiceNotReady
};

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other). Entries should be never modified
// or deleted. Only additions possible.
enum class DeviceSyncSetSoftwareFeature {
  kUnknown = 0,
  kBetterTogetherSuite = 1,
  kSmartLock = 2,
  kInstantTethering = 3,
  kMessages = 4,
  kUnexpectedClientFeature = 5,
  kMaxValue = kUnexpectedClientFeature
};

DeviceSyncRequestFailureReason GetDeviceSyncRequestFailureReason(
    mojom::NetworkRequestResult failure_reason) {
  switch (failure_reason) {
    case mojom::NetworkRequestResult::kRequestSucceededButUnexpectedResult:
      return DeviceSyncRequestFailureReason::
          kRequestSucceededButUnexpectedResult;
    case mojom::NetworkRequestResult::kServiceNotYetInitialized:
      return DeviceSyncRequestFailureReason::kServiceNotYetInitialized;
    case mojom::NetworkRequestResult::kOffline:
      return DeviceSyncRequestFailureReason::kOffline;
    case mojom::NetworkRequestResult::kEndpointNotFound:
      return DeviceSyncRequestFailureReason::kEndpointNotFound;
    case mojom::NetworkRequestResult::kAuthenticationError:
      return DeviceSyncRequestFailureReason::kAuthenticationError;
    case mojom::NetworkRequestResult::kBadRequest:
      return DeviceSyncRequestFailureReason::kBadRequest;
    case mojom::NetworkRequestResult::kResponseMalformed:
      return DeviceSyncRequestFailureReason::kResponseMalformed;
    case mojom::NetworkRequestResult::kInternalServerError:
      return DeviceSyncRequestFailureReason::kInternalServerError;
    case mojom::NetworkRequestResult::kUnknown:
      return DeviceSyncRequestFailureReason::kUnknownNetworkError;
    default:
      return DeviceSyncRequestFailureReason::kUnknown;
  }
  NOTREACHED_IN_MIGRATION();
}

// The exponential back off is: base * 2^(num_failures - 1)
base::TimeDelta GetRetryDelay(size_t num_failures) {
  DCHECK_GT(num_failures, 0u);
  return kBaseRetryDelay * (1 << (num_failures - 1));
}

void RecordGcmRegistrationMetrics(const base::TimeDelta& execution_time,
                                  bool success) {
  base::UmaHistogramCustomTimes(
      "CryptAuth.DeviceSyncService.GcmRegistration.ExecutionTime",
      execution_time, base::Seconds(1) /* min */, base::Minutes(10) /* max */,
      100 /* buckets */);

  base::UmaHistogramBoolean(
      "CryptAuth.DeviceSyncService.GcmRegistration.Success", success);
}

void RecordClientAppMetadataFetchMetrics(const base::TimeDelta& execution_time,
                                         CryptAuthAsyncTaskResult result) {
  base::UmaHistogramCustomTimes(
      "CryptAuth.DeviceSyncService.ClientAppMetadataFetch.ExecutionTime",
      execution_time, base::Seconds(1) /* min */,
      kWaitingForClientAppMetadataTimeout /* max */, 100 /* buckets */);

  LogCryptAuthAsyncTaskSuccessMetric(
      "CryptAuth.DeviceSyncService.ClientAppMetadataFetch.AsyncTaskResult",
      result);
}

void RecordInitializationMetrics(const base::TimeDelta& execution_time) {
  base::UmaHistogramCustomTimes(
      "CryptAuth.DeviceSyncService.Initialization.ExecutionTime",
      execution_time, base::Seconds(1) /* min */,
      kWaitingForClientAppMetadataTimeout /* max */, 100 /* buckets */);
}

void RecordSetSoftwareFeatureStateResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result", success);
}

void RecordSetSoftwareFeatureStateResultFailureReason(
    DeviceSyncRequestFailureReason failure_reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result."
      "FailureReason",
      failure_reason);
}

DeviceSyncSetSoftwareFeature GetDeviceSyncSoftwareFeature(
    multidevice::SoftwareFeature software_feature) {
  switch (software_feature) {
    case multidevice::SoftwareFeature::kBetterTogetherHost:
      return DeviceSyncSetSoftwareFeature::kBetterTogetherSuite;
    case multidevice::SoftwareFeature::kSmartLockHost:
      return DeviceSyncSetSoftwareFeature::kSmartLock;
    case multidevice::SoftwareFeature::kInstantTetheringHost:
      return DeviceSyncSetSoftwareFeature::kInstantTethering;
    case multidevice::SoftwareFeature::kMessagesForWebHost:
      return DeviceSyncSetSoftwareFeature::kMessages;
    default:
      NOTREACHED_IN_MIGRATION();
      return DeviceSyncSetSoftwareFeature::kUnexpectedClientFeature;
  }
}

void RecordSetSoftwareFailedFeature(bool enabled,
                                    multidevice::SoftwareFeature feature) {
  if (enabled) {
    UMA_HISTOGRAM_ENUMERATION(
        "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Enable."
        "FailedFeature",
        GetDeviceSyncSoftwareFeature(feature));
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Disable."
        "FailedFeature",
        GetDeviceSyncSoftwareFeature(feature));
  }
}

void RecordFindEligibleDevicesResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN(
      "MultiDevice.DeviceSyncService.FindEligibleDevices.Result", success);
}

void RecordFindEligibleDevicesResultFailureReason(
    DeviceSyncRequestFailureReason failure_reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "MultiDevice.DeviceSyncService.FindEligibleDevices.Result."
      "FailureReason",
      failure_reason);
}

void RecordForceEnrollmentNowResult(ForceCryptAuthOperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "MultiDevice.DeviceSyncService.ForceEnrollmentNow.Result", result);
}

void RecordForceSyncNowResult(ForceCryptAuthOperationResult result) {
  UMA_HISTOGRAM_ENUMERATION("MultiDevice.DeviceSyncService.ForceSyncNow.Result",
                            result);
}

}  // namespace

// static
DeviceSyncImpl::Factory* DeviceSyncImpl::Factory::custom_factory_instance_ =
    nullptr;

// static
std::unique_ptr<DeviceSyncBase> DeviceSyncImpl::Factory::Create(
    signin::IdentityManager* identity_manager,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* profile_prefs,
    const GcmDeviceInfoProvider* gcm_device_info_provider,
    ClientAppMetadataProvider* client_app_metadata_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<base::OneShotTimer> timer,
    AttestationCertificatesSyncer::GetAttestationCertificatesFunction
        get_attestation_certificates_function) {
  if (custom_factory_instance_) {
    return custom_factory_instance_->CreateInstance(
        identity_manager, gcm_driver, instance_id_driver, profile_prefs,
        gcm_device_info_provider, client_app_metadata_provider,
        std::move(url_loader_factory), std::move(timer),
        get_attestation_certificates_function);
  }

  return base::WrapUnique(new DeviceSyncImpl(
      identity_manager, gcm_driver, instance_id_driver, profile_prefs,
      gcm_device_info_provider, client_app_metadata_provider,
      std::move(url_loader_factory), base::DefaultClock::GetInstance(),
      std::move(timer), get_attestation_certificates_function));
}

// static
void DeviceSyncImpl::Factory::SetCustomFactory(Factory* custom_factory) {
  custom_factory_instance_ = custom_factory;
}

// static
bool DeviceSyncImpl::Factory::IsCustomFactorySet() {
  return custom_factory_instance_ != nullptr;
}

DeviceSyncImpl::Factory::~Factory() = default;

DeviceSyncImpl::PendingSetSoftwareFeatureRequest::
    PendingSetSoftwareFeatureRequest(
        const std::string& device_public_key,
        multidevice::SoftwareFeature software_feature,
        bool enabled,
        RemoteDeviceProvider* remote_device_provider,
        SetSoftwareFeatureStateCallback callback)
    : device_public_key_(device_public_key),
      software_feature_(software_feature),
      enabled_(enabled),
      remote_device_provider_(remote_device_provider),
      callback_(std::move(callback)) {}

DeviceSyncImpl::PendingSetSoftwareFeatureRequest::
    ~PendingSetSoftwareFeatureRequest() = default;

bool DeviceSyncImpl::PendingSetSoftwareFeatureRequest::IsFulfilled() const {
  const auto& synced_devices = remote_device_provider_->GetSyncedDevices();
  const auto devices_it =
      base::ranges::find(synced_devices, device_public_key_,
                         &multidevice::RemoteDevice::public_key);

  // If the device to edit no longer exists, the request is not fulfilled.
  if (devices_it == synced_devices.end())
    return false;

  const auto features_map_it =
      devices_it->software_features.find(software_feature_);

  // If the device does not contain an entry for |software_feature_|, the
  // request is not fulfilled.
  if (features_map_it == devices_it->software_features.end())
    return false;

  if (enabled_)
    return features_map_it->second ==
           multidevice::SoftwareFeatureState::kEnabled;

  return features_map_it->second ==
         multidevice::SoftwareFeatureState::kSupported;
}

void DeviceSyncImpl::PendingSetSoftwareFeatureRequest::InvokeCallback(
    mojom::NetworkRequestResult result) {
  // Callback should only be invoked once.
  DCHECK(callback_);
  std::move(callback_).Run(result);
}

DeviceSyncImpl::PendingSetFeatureStatusRequest::PendingSetFeatureStatusRequest(
    const std::string& device_instance_id,
    multidevice::SoftwareFeature software_feature,
    FeatureStatusChange status_change,
    RemoteDeviceProvider* remote_device_provider,
    SetFeatureStatusCallback callback)
    : device_instance_id_(device_instance_id),
      software_feature_(software_feature),
      status_change_(status_change),
      remote_device_provider_(remote_device_provider),
      callback_(std::move(callback)) {
  DCHECK(!device_instance_id.empty());
}

DeviceSyncImpl::PendingSetFeatureStatusRequest::
    ~PendingSetFeatureStatusRequest() = default;

bool DeviceSyncImpl::PendingSetFeatureStatusRequest::IsFulfilled() const {
  // True if the device from the request is included in the synced-devices list.
  bool is_requested_device_in_list = false;

  // True if the feature from the request is enabled on the device from the
  // request.
  bool is_feature_enabled_for_requested_device = false;

  // True if the feature from the request is enabled on any synced device other
  // than the device from the request.
  bool is_feature_enabled_for_any_other_device = false;

  for (const multidevice::RemoteDevice& remote_device :
       remote_device_provider_->GetSyncedDevices()) {
    const auto it = remote_device.software_features.find(software_feature_);
    bool is_feature_set_for_device =
        it != remote_device.software_features.end();
    bool is_feature_enabled_for_device =
        is_feature_set_for_device &&
        it->second == multidevice::SoftwareFeatureState::kEnabled;

    if (device_instance_id_ == remote_device.instance_id) {
      DCHECK(!is_requested_device_in_list);
      is_requested_device_in_list = true;

      // If the requested device does not contain an entry for
      // |software_feature_|, the request is not fulfilled.
      if (!is_feature_set_for_device)
        return false;

      is_feature_enabled_for_requested_device = is_feature_enabled_for_device;
    } else {
      is_feature_enabled_for_any_other_device =
          is_feature_enabled_for_any_other_device ||
          is_feature_enabled_for_device;
    }
  }

  // If the requested device no longer exists, the request is not fulfilled.
  if (!is_requested_device_in_list)
    return false;

  switch (status_change_) {
    case FeatureStatusChange::kEnableExclusively:
      return is_feature_enabled_for_requested_device &&
             !is_feature_enabled_for_any_other_device;
    case FeatureStatusChange::kEnableNonExclusively:
      return is_feature_enabled_for_requested_device;
    case FeatureStatusChange::kDisable:
      return !is_feature_enabled_for_requested_device;
  }
}

void DeviceSyncImpl::PendingSetFeatureStatusRequest::InvokeCallback(
    mojom::NetworkRequestResult result) {
  // Callback should only be invoked once.
  DCHECK(callback_);
  std::move(callback_).Run(result);
}

DeviceSyncImpl::DeviceSyncImpl(
    signin::IdentityManager* identity_manager,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* profile_prefs,
    const GcmDeviceInfoProvider* gcm_device_info_provider,
    ClientAppMetadataProvider* client_app_metadata_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> timer,
    AttestationCertificatesSyncer::GetAttestationCertificatesFunction
        get_attestation_certificates_function)
    : DeviceSyncBase(),
      identity_manager_(identity_manager),
      gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      profile_prefs_(profile_prefs),
      gcm_device_info_provider_(gcm_device_info_provider),
      client_app_metadata_provider_(client_app_metadata_provider),
      url_loader_factory_(std::move(url_loader_factory)),
      clock_(clock),
      timer_(std::move(timer)),
      get_attestation_certificates_function_(
          get_attestation_certificates_function) {
  DCHECK(profile_prefs_);
  PA_LOG(VERBOSE) << "DeviceSyncImpl: Initializing.";
  initialization_start_timestamp_ = base::TimeTicks::Now();
  RunNextInitializationStep();
}

DeviceSyncImpl::~DeviceSyncImpl() {
  if (cryptauth_enrollment_manager_)
    cryptauth_enrollment_manager_->RemoveObserver(this);

  if (remote_device_provider_)
    remote_device_provider_->RemoveObserver(this);

  if (identity_manager_)
    identity_manager_->RemoveObserver(this);  // No-op if we aren't observing.
}

void DeviceSyncImpl::ForceEnrollmentNow(ForceEnrollmentNowCallback callback) {
  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::ForceEnrollmentNow() invoked before "
                    << "initialization was complete. Cannot force enrollment.";
    std::move(callback).Run(false /* success */);
    RecordForceEnrollmentNowResult(
        ForceCryptAuthOperationResult::kServiceNotReady /* result */);
    return;
  }

  cryptauth_enrollment_manager_->ForceEnrollmentNow(
      cryptauth::INVOCATION_REASON_MANUAL, std::nullopt /* session_id */);
  std::move(callback).Run(true /* success */);
  RecordForceEnrollmentNowResult(
      ForceCryptAuthOperationResult::kSuccess /* result */);
}

void DeviceSyncImpl::ForceSyncNow(ForceSyncNowCallback callback) {
  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::ForceSyncNow() invoked before "
                    << "initialization was complete. Cannot force sync.";
    std::move(callback).Run(false /* success */);
    RecordForceSyncNowResult(
        ForceCryptAuthOperationResult::kServiceNotReady /* result */);
    return;
  }

  if (features::ShouldUseV2DeviceSync()) {
    cryptauth_v2_device_manager_->ForceDeviceSyncNow(
        cryptauthv2::ClientMetadata::MANUAL, std::nullopt /* session_id */);
  }

  std::move(callback).Run(true /* success */);
  RecordForceSyncNowResult(
      ForceCryptAuthOperationResult::kSuccess /* result */);
}

void DeviceSyncImpl::GetGroupPrivateKeyStatus(
    GetGroupPrivateKeyStatusCallback callback) {
  DCHECK(features::ShouldUseV2DeviceSync);

  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::GetGroupPrivateKeyStatus() invoked "
                       "before initialization was complete. Cannot return "
                       "group private key status.";
    std::move(callback).Run(
        GroupPrivateKeyStatus::
            kStatusUnavailableBecauseDeviceSyncIsNotInitialized);
    return;
  }

  std::move(callback).Run(
      cryptauth_v2_device_manager_->GetDeviceSyncerGroupPrivateKeyStatus());
}

void DeviceSyncImpl::GetBetterTogetherMetadataStatus(
    GetBetterTogetherMetadataStatusCallback callback) {
  DCHECK(features::ShouldUseV2DeviceSync);

  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING)
        << "DeviceSyncImpl::GetBetterTogetherMetadataStatus() invoked "
           "before initialization was complete. Cannot return "
           "better together metadata status.";
    std::move(callback).Run(
        BetterTogetherMetadataStatus::
            kStatusUnavailableBecauseDeviceSyncIsNotInitialized);
    return;
  }

  std::move(callback).Run(cryptauth_v2_device_manager_
                              ->GetDeviceSyncerBetterTogetherMetadataStatus());
}

void DeviceSyncImpl::GetLocalDeviceMetadata(
    GetLocalDeviceMetadataCallback callback) {
  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::GetLocalDeviceMetadata() invoked "
                    << "before initialization was complete. Cannot return "
                    << "local device metadata.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::string public_key = cryptauth_enrollment_manager_->GetUserPublicKey();
  DCHECK(!public_key.empty());
  std::move(callback).Run(GetSyncedDeviceWithPublicKey(public_key));
}

void DeviceSyncImpl::GetSyncedDevices(GetSyncedDevicesCallback callback) {
  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::GetSyncedDevices() invoked before "
                    << "initialization was complete. Cannot return devices.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(remote_device_provider_->GetSyncedDevices());
}

void DeviceSyncImpl::SetSoftwareFeatureState(
    const std::string& device_public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    SetSoftwareFeatureStateCallback callback) {
  // This method is deprecated.
  std::move(callback).Run(mojom::NetworkRequestResult::kUnknown);
  return;
}

void DeviceSyncImpl::SetFeatureStatus(const std::string& device_instance_id,
                                      multidevice::SoftwareFeature feature,
                                      FeatureStatusChange status_change,
                                      SetFeatureStatusCallback callback) {
  DCHECK(features::ShouldUseV2DeviceSync);
  DCHECK(!device_instance_id.empty());

  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::SetFeatureStatus() invoked before "
                    << "initialization was complete. Cannot enable/disable "
                    << "feature.";
    std::move(callback).Run(
        mojom::NetworkRequestResult::kServiceNotYetInitialized);
    return;
  }

  auto request_id = base::UnguessableToken::Create();
  id_to_pending_set_feature_status_request_map_.emplace(
      request_id, std::make_unique<PendingSetFeatureStatusRequest>(
                      device_instance_id, feature, status_change,
                      remote_device_provider_.get(), std::move(callback)));

  feature_status_setter_->SetFeatureStatus(
      device_instance_id, feature, status_change,
      base::BindOnce(&DeviceSyncImpl::OnSetFeatureStatusSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DeviceSyncImpl::OnSetFeatureStatusError,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void DeviceSyncImpl::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  // This method is deprecated.
  std::move(callback).Run(mojom::NetworkRequestResult::kUnknown, nullptr);
  return;
}

void DeviceSyncImpl::NotifyDevices(
    const std::vector<std::string>& device_instance_ids,
    cryptauthv2::TargetService target_service,
    multidevice::SoftwareFeature feature,
    NotifyDevicesCallback callback) {
  DCHECK(features::ShouldUseV2DeviceSync);

  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::NotifyDevices() invoked before "
                    << "initialization was complete. Cannot notify devices.";
    std::move(callback).Run(
        mojom::NetworkRequestResult::kServiceNotYetInitialized);
    return;
  }

  auto request_id = base::UnguessableToken::Create();
  pending_notify_devices_callbacks_.emplace(request_id, std::move(callback));

  device_notifier_->NotifyDevices(
      device_instance_ids, target_service,
      CryptAuthFeatureTypeFromSoftwareFeature(feature),
      base::BindOnce(&DeviceSyncImpl::OnNotifyDevicesSuccess,
                     weak_ptr_factory_.GetWeakPtr(), request_id),
      base::BindOnce(&DeviceSyncImpl::OnNotifyDevicesError,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void DeviceSyncImpl::GetDevicesActivityStatus(
    GetDevicesActivityStatusCallback callback) {
  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING)
        << "DeviceSyncImpl::GetDevicesActivityStatus() invoked before "
        << "initialization was complete. Cannot get activity statuses.";
    std::move(callback).Run(
        mojom::NetworkRequestResult::kServiceNotYetInitialized,
        std::nullopt /* device_activity_statuses */);
    return;
  }

  auto request_id = base::UnguessableToken::Create();
  get_devices_activity_status_callbacks_.emplace(request_id,
                                                 std::move(callback));

  cryptauth_device_activity_getter_ =
      CryptAuthDeviceActivityGetterImpl::Factory::Create(
          client_app_metadata_->instance_id(),
          client_app_metadata_->instance_id_token(),
          cryptauth_client_factory_.get());

  cryptauth_device_activity_getter_->GetDevicesActivityStatus(
      base::BindOnce(&DeviceSyncImpl::OnGetDevicesActivityStatusFinished,
                     weak_ptr_factory_.GetWeakPtr(), request_id),
      base::BindOnce(&DeviceSyncImpl::OnGetDevicesActivityStatusError,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void DeviceSyncImpl::GetDebugInfo(GetDebugInfoCallback callback) {
  if (status_ != InitializationStatus::kReady) {
    PA_LOG(WARNING) << "DeviceSyncImpl::GetDebugInfo() invoked before "
                    << "initialization was complete. Cannot provide info.";
    std::move(callback).Run(nullptr);
    return;
  }

  if (features::ShouldUseV2DeviceSync()) {
    std::move(callback).Run(mojom::DebugInfo::New(
        cryptauth_enrollment_manager_->GetLastEnrollmentTime(),
        cryptauth_enrollment_manager_->GetTimeToNextAttempt(),
        cryptauth_enrollment_manager_->IsRecoveringFromFailure(),
        cryptauth_enrollment_manager_->IsEnrollmentInProgress(),
        cryptauth_v2_device_manager_->GetLastDeviceSyncTime().value_or(
            base::Time()),
        cryptauth_v2_device_manager_->GetTimeToNextAttempt().value_or(
            base::TimeDelta::Max()),
        cryptauth_v2_device_manager_->IsRecoveringFromFailure(),
        cryptauth_v2_device_manager_->IsDeviceSyncInProgress()));
  }
}

void DeviceSyncImpl::OnEnrollmentFinished(bool success) {
  PA_LOG(VERBOSE) << "DeviceSyncImpl: Enrollment finished; success = "
                  << success;

  if (!success)
    return;

  if (status_ == InitializationStatus::kWaitingForEnrollment)
    RunNextInitializationStep();

  NotifyOnEnrollmentFinished();
}

void DeviceSyncImpl::OnSyncDeviceListChanged() {
  PA_LOG(VERBOSE) << "DeviceSyncImpl: Synced devices changed; notifying "
                  << "observers.";
  NotifyOnNewDevicesSynced();

  // Iterate through pending SetSoftwareFeature() requests. If any of them have
  // been fulfilled, invoke their callbacks.
  for (auto it = id_to_pending_set_software_feature_request_map_.begin();
       it != id_to_pending_set_software_feature_request_map_.end();) {
    if (!it->second->IsFulfilled()) {
      ++it;
      continue;
    }

    PA_LOG(VERBOSE)
        << "DeviceSyncImpl::OnSyncDeviceListChanged(): Feature state "
        << "updated via device sync; notifying success callbacks.";
    it->second->InvokeCallback(mojom::NetworkRequestResult::kSuccess);
    it = id_to_pending_set_software_feature_request_map_.erase(it);
  }

  // Iterate through pending SetFeatureStatus() requests. If any of them have
  // been fulfilled, invoke their callbacks.
  for (auto it = id_to_pending_set_feature_status_request_map_.begin();
       it != id_to_pending_set_feature_status_request_map_.end();) {
    if (!it->second->IsFulfilled()) {
      ++it;
      continue;
    }

    PA_LOG(VERBOSE) << "DeviceSyncImpl::OnSyncDeviceListChanged(): Feature "
                    << "status updated via device sync; notifying success "
                    << "callbacks.";
    it->second->InvokeCallback(mojom::NetworkRequestResult::kSuccess);
    it = id_to_pending_set_feature_status_request_map_.erase(it);
  }
}

void DeviceSyncImpl::Shutdown() {
  cryptauth_device_activity_getter_.reset();
  feature_status_setter_.reset();
  device_notifier_.reset();
  remote_device_provider_.reset();
  cryptauth_enrollment_manager_.reset();
  cryptauth_v2_device_manager_.reset();
  cryptauth_device_registry_.reset();
  cryptauth_scheduler_.reset();
  cryptauth_key_registry_.reset();
  cryptauth_client_factory_.reset();
  cryptauth_gcm_manager_.reset();

  identity_manager_ = nullptr;
  gcm_driver_ = nullptr;
  instance_id_driver_ = nullptr;
  profile_prefs_ = nullptr;
  gcm_device_info_provider_ = nullptr;
  client_app_metadata_provider_ = nullptr;
  url_loader_factory_ = nullptr;
  clock_ = nullptr;
}

void DeviceSyncImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  PA_LOG(VERBOSE) << "DeviceSyncImpl: OnPrimaryAccountChanged";
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      identity_manager_->RemoveObserver(this);
      ProcessPrimaryAccountInfo(event.GetCurrentState().primary_account);
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [[fallthrough]];
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      // Ignored
      break;
  }
}

void DeviceSyncImpl::RunNextInitializationStep() {
  switch (status_) {
    case InitializationStatus::kNotStarted:
      FetchAccountInfo();
      break;
    case InitializationStatus::kFetchingAccountInfo:
      RegisterWithGcm();
      break;
    case InitializationStatus::kWaitingForGcmRegistration:
      FetchClientAppMetadata();
      break;
    case InitializationStatus::kWaitingForClientAppMetadata:
      WaitForValidEnrollment();
      break;
    case InitializationStatus::kWaitingForEnrollment:
      CompleteInitializationAfterSuccessfulEnrollment();
      break;
    case InitializationStatus::kReady:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void DeviceSyncImpl::FetchAccountInfo() {
  status_ = InitializationStatus::kFetchingAccountInfo;

  // "Unconsented" because this feature is not tied to browser sync consent.
  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.account_id.empty()) {
    // Primary profile not loaded yet. This happens when adding a new account.
    PA_LOG(VERBOSE) << "DeviceSyncImpl: Waiting for primary account info";
    identity_manager_->AddObserver(this);
  } else {
    // Profile is ready immediately. This occurs during normal login and during
    // the browser crash-and-restore flow.
    ProcessPrimaryAccountInfo(primary_account);
  }
}

void DeviceSyncImpl::ProcessPrimaryAccountInfo(
    const CoreAccountInfo& primary_account_info) {
  DCHECK_EQ(status_, InitializationStatus::kFetchingAccountInfo);
  if (primary_account_info.account_id.empty()) {
    PA_LOG(ERROR)
        << "No primary account information available; cannot proceed.";

    // TODO(jamescook): This early exit was originally added to work around
    // browser_tests failures. Those don't happen any more. However, I am
    // uncertain how primary account ids work for non-GAIA logins like Active
    // Directory, and I can't figure out how to test them, so I'm leaving
    // this here.
    return;
  }

  primary_account_info_ = primary_account_info;
  PA_LOG(VERBOSE) << "DeviceSyncImpl: Profile initialized.";

  RunNextInitializationStep();
}

void DeviceSyncImpl::RegisterWithGcm() {
  status_ = InitializationStatus::kWaitingForGcmRegistration;

  if (!cryptauth_gcm_manager_) {
    // Initialize |cryptauth_gcm_manager_| and have it start listening for GCM
    // tickles.
    cryptauth_gcm_manager_ = CryptAuthGCMManagerImpl::Factory::Create(
        gcm_driver_, instance_id_driver_, profile_prefs_);
    cryptauth_gcm_manager_->StartListening();
  }

  // The device previously completed GCM registration, and that registration
  // id is not deprecated.
  const std::string& registration_id =
      cryptauth_gcm_manager_->GetRegistrationId();
  if (!registration_id.empty() &&
      !CryptAuthGCMManager::IsRegistrationIdDeprecated(registration_id)) {
    RunNextInitializationStep();
    return;
  }

  // The registration result is sent to observers via OnGCMRegistrationResult().
  cryptauth_gcm_manager_->AddObserver(this);

  DCHECK(cryptauth_gcm_manager_->IsListening());
  PA_LOG(VERBOSE) << "DeviceSyncImpl: Registering with GCM.";
  gcm_registration_start_timestamp_ = base::TimeTicks::Now();
  cryptauth_gcm_manager_->RegisterWithGCM();
}

void DeviceSyncImpl::OnGCMRegistrationResult(bool success) {
  if (status_ != InitializationStatus::kWaitingForGcmRegistration)
    return;

  cryptauth_gcm_manager_->RemoveObserver(this);

  bool was_successful =
      success && !cryptauth_gcm_manager_->GetRegistrationId().empty();
  RecordGcmRegistrationMetrics(
      base::TimeTicks::Now() - gcm_registration_start_timestamp_,
      was_successful);

  if (!was_successful) {
    ++num_gcm_registration_failures_;
    PA_LOG(ERROR) << "DeviceSyncImpl: GCM registration failed. Num failures: "
                  << num_gcm_registration_failures_;
    timer_->Start(FROM_HERE, GetRetryDelay(num_gcm_registration_failures_),
                  base::BindOnce(&DeviceSyncImpl::RegisterWithGcm,
                                 base::Unretained(this)));
    return;
  }

  PA_LOG(VERBOSE) << "DeviceSyncImpl: GCM registration succeeded.";
  RunNextInitializationStep();
}

void DeviceSyncImpl::FetchClientAppMetadata() {
  status_ = InitializationStatus::kWaitingForClientAppMetadata;

  timer_->Start(FROM_HERE, kWaitingForClientAppMetadataTimeout,
                base::BindOnce(&DeviceSyncImpl::OnClientAppMetadataFetchTimeout,
                               weak_ptr_factory_.GetWeakPtr()));

  // NOTE(crbug.com/1006280): It is very important that the GCM manager is
  // listening before fetching ClientAppMetadata. Otherwise, the user's Instance
  // ID, which is assumed to be stable, might be deleted.
  DCHECK(cryptauth_gcm_manager_->IsListening());
  PA_LOG(VERBOSE) << "DeviceSyncImpl: Fetching ClientAppMetadata.";
  client_app_metadata_fetch_start_timestamp_ = base::TimeTicks::Now();
  client_app_metadata_provider_->GetClientAppMetadata(
      cryptauth_gcm_manager_->GetRegistrationId(),
      base::BindOnce(&DeviceSyncImpl::OnClientAppMetadataFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceSyncImpl::OnClientAppMetadataFetchTimeout() {
  DCHECK_EQ(status_, InitializationStatus::kWaitingForClientAppMetadata);
  weak_ptr_factory_.InvalidateWeakPtrs();
  PA_LOG(VERBOSE) << "DeviceSyncImpl: ClientAppMetadata fetch timed out.";
  RecordClientAppMetadataFetchMetrics(
      base::TimeTicks::Now() - client_app_metadata_fetch_start_timestamp_,
      CryptAuthAsyncTaskResult::kTimeout);

  OnClientAppMetadataFetchFailure();
}

void DeviceSyncImpl::OnClientAppMetadataFetched(
    const std::optional<cryptauthv2::ClientAppMetadata>& client_app_metadata) {
  DCHECK_EQ(status_, InitializationStatus::kWaitingForClientAppMetadata);
  timer_->Stop();

  bool success = client_app_metadata.has_value();
  CryptAuthAsyncTaskResult result = success ? CryptAuthAsyncTaskResult::kSuccess
                                            : CryptAuthAsyncTaskResult::kError;
  RecordClientAppMetadataFetchMetrics(
      base::TimeTicks::Now() - client_app_metadata_fetch_start_timestamp_,
      result);

  if (!success) {
    OnClientAppMetadataFetchFailure();
    return;
  }

  client_app_metadata_ = client_app_metadata;
  RunNextInitializationStep();
}

void DeviceSyncImpl::OnClientAppMetadataFetchFailure() {
  DCHECK_EQ(status_, InitializationStatus::kWaitingForClientAppMetadata);
  ++num_client_app_metadata_fetch_failures_;
  PA_LOG(ERROR)
      << "DeviceSyncImpl: ClientAppMetadata fetch failed. Num failures: "
      << num_client_app_metadata_fetch_failures_;
  timer_->Start(FROM_HERE,
                GetRetryDelay(num_client_app_metadata_fetch_failures_),
                base::BindOnce(&DeviceSyncImpl::FetchClientAppMetadata,
                               base::Unretained(this)));
}

void DeviceSyncImpl::WaitForValidEnrollment() {
  status_ = InitializationStatus::kWaitingForEnrollment;
  InitializeCryptAuthManagementObjects();

  // If enrollment has not yet completed successfully, initialization cannot
  // continue. Once enrollment has finished, OnEnrollmentFinished() is invoked,
  // which finishes the initialization flow.
  if (!cryptauth_enrollment_manager_->IsEnrollmentValid()) {
    PA_LOG(VERBOSE) << "DeviceSyncImpl: Waiting for enrollment to complete.";
    return;
  }

  RunNextInitializationStep();
}

void DeviceSyncImpl::InitializeCryptAuthManagementObjects() {
  DCHECK_EQ(status_, InitializationStatus::kWaitingForEnrollment);

  cryptauth_client_factory_ = std::make_unique<CryptAuthClientFactoryImpl>(
      identity_manager_, url_loader_factory_,
      device_classifier_util::GetDeviceClassifier());

  // Initialize |cryptauth_enrollment_manager_| and start observing, then call
  // Start() immediately to schedule enrollment.
  cryptauth_key_registry_ =
      CryptAuthKeyRegistryImpl::Factory::Create(profile_prefs_);

  cryptauth_scheduler_ =
      CryptAuthSchedulerImpl::Factory::Create(profile_prefs_);

  cryptauth_enrollment_manager_ =
      CryptAuthV2EnrollmentManagerImpl::Factory::Create(
          *client_app_metadata_, cryptauth_key_registry_.get(),
          cryptauth_client_factory_.get(), cryptauth_gcm_manager_.get(),
          cryptauth_scheduler_.get(), profile_prefs_, clock_);

  // Start() is not called yet since the device has not completed enrollment.
  if (features::ShouldUseV2DeviceSync()) {
    cryptauth_device_registry_ =
        CryptAuthDeviceRegistryImpl::Factory::Create(profile_prefs_);

    cryptauth_v2_device_manager_ =
        CryptAuthV2DeviceManagerImpl::Factory::Create(
            *client_app_metadata_, cryptauth_device_registry_.get(),
            cryptauth_key_registry_.get(), cryptauth_client_factory_.get(),
            cryptauth_gcm_manager_.get(), cryptauth_scheduler_.get(),
            profile_prefs_, get_attestation_certificates_function_);
  }

  cryptauth_enrollment_manager_->AddObserver(this);
  cryptauth_enrollment_manager_->Start();
}

void DeviceSyncImpl::CompleteInitializationAfterSuccessfulEnrollment() {
  DCHECK_EQ(status_, InitializationStatus::kWaitingForEnrollment);
  DCHECK(cryptauth_enrollment_manager_->IsEnrollmentValid());

  // Now that enrollment has completed, the current device has been registered
  // with the CryptAuth back-end and can begin monitoring synced devices.
  if (features::ShouldUseV2DeviceSync()) {
    cryptauth_v2_device_manager_->Start();
  }

  remote_device_provider_ = RemoteDeviceProviderImpl::Factory::Create(
      cryptauth_v2_device_manager_.get(), primary_account_info_.email,
      cryptauth_enrollment_manager_->GetUserPrivateKey());
  remote_device_provider_->AddObserver(this);

  if (features::ShouldUseV2DeviceSync()) {
    feature_status_setter_ = CryptAuthFeatureStatusSetterImpl::Factory::Create(
        client_app_metadata_->instance_id(),
        client_app_metadata_->instance_id_token(),
        cryptauth_client_factory_.get());

    device_notifier_ = CryptAuthDeviceNotifierImpl::Factory::Create(
        client_app_metadata_->instance_id(),
        client_app_metadata_->instance_id_token(),
        cryptauth_client_factory_.get());
  }

  status_ = InitializationStatus::kReady;
  PA_LOG(VERBOSE) << "DeviceSyncImpl: CryptAuth Enrollment is valid; service "
                  << "fully initialized.";
  RecordInitializationMetrics(base::TimeTicks::Now() -
                              initialization_start_timestamp_);
}

std::optional<multidevice::RemoteDevice>
DeviceSyncImpl::GetSyncedDeviceWithPublicKey(
    const std::string& public_key) const {
  DCHECK_EQ(status_, InitializationStatus::kReady)
      << "DeviceSyncImpl::GetSyncedDeviceWithPublicKey() called before ready.";
  const auto& synced_devices = remote_device_provider_->GetSyncedDevices();
  const auto it = base::ranges::find(synced_devices, public_key,
                                     &multidevice::RemoteDevice::public_key);

  if (it == synced_devices.end())
    return std::nullopt;

  return *it;
}

void DeviceSyncImpl::OnSetSoftwareFeatureStateSuccess() {
  DCHECK(features::ShouldUseV1DeviceSync());

  PA_LOG(VERBOSE) << "DeviceSyncImpl::OnSetSoftwareFeatureStateSuccess(): "
                  << "Successfully completed SetSoftwareFeatureState() call; "
                  << "requesting force sync.";

  if (features::ShouldUseV2DeviceSync()) {
    cryptauth_v2_device_manager_->ForceDeviceSyncNow(
        cryptauthv2::ClientMetadata::FEATURE_TOGGLED,
        std::nullopt /* session_id */);
  }

  RecordSetSoftwareFeatureStateResult(true /* success */);
}

void DeviceSyncImpl::OnSetSoftwareFeatureStateError(
    const base::UnguessableToken& request_id,
    NetworkRequestError error) {
  DCHECK(features::ShouldUseV1DeviceSync());

  auto it = id_to_pending_set_software_feature_request_map_.find(request_id);
  if (it == id_to_pending_set_software_feature_request_map_.end()) {
    PA_LOG(ERROR) << "DeviceSyncImpl::OnSetSoftwareFeatureStateError(): "
                  << "Could not find request entry with ID " << request_id;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  RecordSetSoftwareFeatureStateResult(false /* success */);
  RecordSetSoftwareFeatureStateResultFailureReason(
      GetDeviceSyncRequestFailureReason(
          mojo::ConvertTo<mojom::NetworkRequestResult>(error)));
  RecordSetSoftwareFailedFeature(it->second->enabled(),
                                 it->second->software_feature());

  it->second->InvokeCallback(
      mojo::ConvertTo<mojom::NetworkRequestResult>(error));
  id_to_pending_set_software_feature_request_map_.erase(it);
}

void DeviceSyncImpl::OnSetFeatureStatusSuccess() {
  DCHECK(features::ShouldUseV2DeviceSync());

  PA_LOG(VERBOSE) << "DeviceSyncImpl::OnSetFeatureStatusSuccess(): "
                  << "Successfully completed SetFeatureStatus() call; "
                  << "requesting force sync.";

  cryptauth_v2_device_manager_->ForceDeviceSyncNow(
      cryptauthv2::ClientMetadata::FEATURE_TOGGLED,
      std::nullopt /* session_id */);
}

void DeviceSyncImpl::OnSetFeatureStatusError(
    const base::UnguessableToken& request_id,
    NetworkRequestError error) {
  DCHECK(features::ShouldUseV2DeviceSync());

  auto it = id_to_pending_set_feature_status_request_map_.find(request_id);
  if (it == id_to_pending_set_feature_status_request_map_.end()) {
    PA_LOG(ERROR) << "DeviceSyncImpl::OnSetFeatureStatusError(): "
                  << "Could not find request entry with ID " << request_id;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  it->second->InvokeCallback(
      mojo::ConvertTo<mojom::NetworkRequestResult>(error));
  id_to_pending_set_feature_status_request_map_.erase(it);
}

void DeviceSyncImpl::OnFindEligibleDevicesSuccess(
    base::OnceCallback<void(mojom::NetworkRequestResult,
                            mojom::FindEligibleDevicesResponsePtr)> callback,
    const std::vector<cryptauth::ExternalDeviceInfo>& eligible_device_infos,
    const std::vector<cryptauth::IneligibleDevice>& ineligible_devices) {
  DCHECK(features::ShouldUseV1DeviceSync());

  std::vector<multidevice::RemoteDevice> eligible_remote_devices;
  for (const auto& eligible_device_info : eligible_device_infos) {
    auto possible_device =
        GetSyncedDeviceWithPublicKey(eligible_device_info.public_key());
    if (possible_device) {
      eligible_remote_devices.emplace_back(*possible_device);
    } else {
      PA_LOG(ERROR) << "Could not find eligible device with public key \""
                    << eligible_device_info.public_key() << "\".";
    }
  }

  std::vector<multidevice::RemoteDevice> ineligible_remote_devices;
  for (const auto& ineligible_device : ineligible_devices) {
    auto possible_device =
        GetSyncedDeviceWithPublicKey(ineligible_device.device().public_key());
    if (possible_device) {
      ineligible_remote_devices.emplace_back(*possible_device);
    } else {
      PA_LOG(ERROR) << "Could not find ineligible device with public key \""
                    << ineligible_device.device().public_key() << "\".";
    }
  }

  std::move(callback).Run(
      mojom::NetworkRequestResult::kSuccess,
      mojom::FindEligibleDevicesResponse::New(eligible_remote_devices,
                                              ineligible_remote_devices));

  RecordFindEligibleDevicesResult(true /* success */);
}

void DeviceSyncImpl::OnFindEligibleDevicesError(
    base::OnceCallback<void(mojom::NetworkRequestResult,
                            mojom::FindEligibleDevicesResponsePtr)> callback,
    NetworkRequestError error) {
  DCHECK(features::ShouldUseV1DeviceSync());

  std::move(callback).Run(mojo::ConvertTo<mojom::NetworkRequestResult>(error),
                          nullptr /* response */);

  RecordFindEligibleDevicesResult(false /* success */);
  RecordFindEligibleDevicesResultFailureReason(
      GetDeviceSyncRequestFailureReason(
          mojo::ConvertTo<mojom::NetworkRequestResult>(error)));
}

void DeviceSyncImpl::OnNotifyDevicesSuccess(
    const base::UnguessableToken& request_id) {
  DCHECK(features::ShouldUseV2DeviceSync());

  auto it = pending_notify_devices_callbacks_.find(request_id);
  if (it == pending_notify_devices_callbacks_.end()) {
    PA_LOG(ERROR) << "DeviceSyncImpl::OnNotifyDevicesSuccess(): "
                  << "Could not find request entry with ID " << request_id;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::move(it->second).Run(mojom::NetworkRequestResult::kSuccess);
  pending_notify_devices_callbacks_.erase(it);
}

void DeviceSyncImpl::OnNotifyDevicesError(
    const base::UnguessableToken& request_id,
    NetworkRequestError error) {
  DCHECK(features::ShouldUseV2DeviceSync());

  auto it = pending_notify_devices_callbacks_.find(request_id);
  if (it == pending_notify_devices_callbacks_.end()) {
    PA_LOG(ERROR) << "DeviceSyncImpl::OnNotifyDevicesError(): "
                  << "Could not find request entry with ID " << request_id;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::move(it->second)
      .Run(mojo::ConvertTo<mojom::NetworkRequestResult>(error));
  pending_notify_devices_callbacks_.erase(it);
}

void DeviceSyncImpl::OnGetDevicesActivityStatusFinished(
    const base::UnguessableToken& request_id,
    CryptAuthDeviceActivityGetter::DeviceActivityStatusResult
        device_activity_status_result) {
  DCHECK(features::ShouldUseV2DeviceSync());

  auto iter = get_devices_activity_status_callbacks_.find(request_id);
  DCHECK(iter != get_devices_activity_status_callbacks_.end());
  std::move(iter->second)
      .Run(mojom::NetworkRequestResult::kSuccess,
           std::make_optional(std::move(device_activity_status_result)));
  get_devices_activity_status_callbacks_.erase(iter);
}

void DeviceSyncImpl::OnGetDevicesActivityStatusError(
    const base::UnguessableToken& request_id,
    NetworkRequestError error) {
  DCHECK(features::ShouldUseV2DeviceSync());

  auto iter = get_devices_activity_status_callbacks_.find(request_id);
  DCHECK(iter != get_devices_activity_status_callbacks_.end());
  std::move(iter->second)
      .Run(mojo::ConvertTo<mojom::NetworkRequestResult>(error), std::nullopt);
  get_devices_activity_status_callbacks_.erase(iter);
}

void DeviceSyncImpl::StartSetSoftwareFeatureTimer() {
  timer_->Start(FROM_HERE, kSetFeatureEnabledTimeout,
                base::BindOnce(&DeviceSyncImpl::OnSetSoftwareFeatureTimerFired,
                               base::Unretained(this)));
}

void DeviceSyncImpl::OnSetSoftwareFeatureTimerFired() {
  if (id_to_pending_set_software_feature_request_map_.empty())
    return;

  PA_LOG(WARNING) << "DeviceSyncImpl::OnSetSoftwareFeatureTimerFired(): "
                  << "Timed out waiting for device feature states to update. "
                  << "Invoking failure callbacks.";

  // Any pending requests that are still present have timed out, so invoke
  // their callbacks and remove them from the map.
  auto it = id_to_pending_set_software_feature_request_map_.begin();
  while (it != id_to_pending_set_software_feature_request_map_.end()) {
    RecordSetSoftwareFeatureStateResult(false /* success */);
    RecordSetSoftwareFeatureStateResultFailureReason(
        DeviceSyncRequestFailureReason::kRequestSucceededButUnexpectedResult);
    RecordSetSoftwareFailedFeature(it->second->enabled(),
                                   it->second->software_feature());

    it->second->InvokeCallback(
        mojom::NetworkRequestResult::kRequestSucceededButUnexpectedResult);
    it = id_to_pending_set_software_feature_request_map_.erase(it);
  }
}

}  // namespace device_sync

}  // namespace ash
