// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "chromeos/ash/services/multidevice_setup/multidevice_setup_impl.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/ash/services/multidevice_setup/android_sms_app_installing_status_observer.h"
#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider_impl.h"
#include "chromeos/ash/services/multidevice_setup/feature_state_manager_impl.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager_impl.h"
#include "chromeos/ash/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"
#include "chromeos/ash/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/ash/services/multidevice_setup/host_device_timestamp_manager_impl.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider_impl.h"
#include "chromeos/ash/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/auth_token_validator.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/ash/services/multidevice_setup/wifi_sync_notification_controller.h"

namespace ash {

namespace multidevice_setup {

namespace {

const char kTestDeviceNameForDebugNotification[] = "Test Device";

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other). Entries should be never modified
// or deleted. Only additions possible.
enum class VerifyAndForgetHostConfirmationState {
  kButtonClickedState = 0,
  kCompletedSetupState = 1,
  kMaxValue = kCompletedSetupState,
};

static void LogForgetHostConfirmed(VerifyAndForgetHostConfirmationState state) {
  UMA_HISTOGRAM_ENUMERATION("MultiDevice.ForgetHostConfirmed", state);
}

static void LogVerifyButtonClicked(VerifyAndForgetHostConfirmationState state) {
  UMA_HISTOGRAM_ENUMERATION("MultiDevice.VerifyButtonClicked", state);
}

}  // namespace

// static
MultiDeviceSetupImpl::Factory* MultiDeviceSetupImpl::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<MultiDeviceSetupBase> MultiDeviceSetupImpl::Factory::Create(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AuthTokenValidator* auth_token_validator,
    OobeCompletionTracker* oobe_completion_tracker,
    AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
    AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
    const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
    bool is_secondary_user) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        pref_service, device_sync_client, auth_token_validator,
        oobe_completion_tracker, android_sms_app_helper_delegate,
        android_sms_pairing_state_tracker, gcm_device_info_provider,
        is_secondary_user);
  }

  return base::WrapUnique(new MultiDeviceSetupImpl(
      pref_service, device_sync_client, auth_token_validator,
      oobe_completion_tracker, android_sms_app_helper_delegate,
      android_sms_pairing_state_tracker, gcm_device_info_provider,
      is_secondary_user));
}

// static
void MultiDeviceSetupImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupImpl::Factory::~Factory() = default;

MultiDeviceSetupImpl::MultiDeviceSetupImpl(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AuthTokenValidator* auth_token_validator,
    OobeCompletionTracker* oobe_completion_tracker,
    AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
    AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
    const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
    bool is_secondary_user)
    : eligible_host_devices_provider_(
          EligibleHostDevicesProviderImpl::Factory::Create(device_sync_client)),
      host_backend_delegate_(HostBackendDelegateImpl::Factory::Create(
          eligible_host_devices_provider_.get(),
          pref_service,
          device_sync_client)),
      host_verifier_(
          HostVerifierImpl::Factory::Create(host_backend_delegate_.get(),
                                            device_sync_client,
                                            pref_service)),
      host_status_provider_(HostStatusProviderImpl::Factory::Create(
          eligible_host_devices_provider_.get(),
          host_backend_delegate_.get(),
          host_verifier_.get(),
          device_sync_client)),
      grandfathered_easy_unlock_host_disabler_(
          GrandfatheredEasyUnlockHostDisabler::Factory::Create(
              host_backend_delegate_.get(),
              device_sync_client,
              pref_service)),
      host_device_timestamp_manager_(
          HostDeviceTimestampManagerImpl::Factory::Create(
              host_status_provider_.get(),
              pref_service,
              base::DefaultClock::GetInstance())),
      delegate_notifier_(
          AccountStatusChangeDelegateNotifierImpl::Factory::Create(
              host_status_provider_.get(),
              pref_service,
              host_device_timestamp_manager_.get(),
              oobe_completion_tracker,
              base::DefaultClock::GetInstance())),
      wifi_sync_feature_manager_(GlobalStateFeatureManagerImpl::Factory::Create(
          GlobalStateFeatureManagerImpl::Factory::Option::kWifiSync,
          host_status_provider_.get(),
          pref_service,
          device_sync_client)),
      wifi_sync_notification_controller_(
          WifiSyncNotificationController::Factory::Create(
              wifi_sync_feature_manager_.get(),
              host_status_provider_.get(),
              pref_service,
              device_sync_client,
              delegate_notifier_.get())),
      feature_state_manager_(FeatureStateManagerImpl::Factory::Create(
          pref_service,
          host_status_provider_.get(),
          device_sync_client,
          android_sms_pairing_state_tracker,
          {{mojom::Feature::kWifiSync, wifi_sync_feature_manager_.get()}},
          is_secondary_user)),
      android_sms_app_installing_host_observer_(
          android_sms_app_helper_delegate
              ? AndroidSmsAppInstallingStatusObserver::Factory::Create(
                    host_status_provider_.get(),
                    feature_state_manager_.get(),
                    android_sms_app_helper_delegate,
                    pref_service)
              : nullptr),
      auth_token_validator_(auth_token_validator) {
  host_status_provider_->AddObserver(this);
  feature_state_manager_->AddObserver(this);
}

MultiDeviceSetupImpl::~MultiDeviceSetupImpl() {
  host_status_provider_->RemoveObserver(this);
  feature_state_manager_->RemoveObserver(this);
}

void MultiDeviceSetupImpl::SetAccountStatusChangeDelegate(
    mojo::PendingRemote<mojom::AccountStatusChangeDelegate> delegate) {
  delegate_notifier_->SetAccountStatusChangeDelegateRemote(std::move(delegate));
}

void MultiDeviceSetupImpl::AddHostStatusObserver(
    mojo::PendingRemote<mojom::HostStatusObserver> observer) {
  host_status_observers_.Add(std::move(observer));
}

void MultiDeviceSetupImpl::AddFeatureStateObserver(
    mojo::PendingRemote<mojom::FeatureStateObserver> observer) {
  feature_state_observers_.Add(std::move(observer));
}

void MultiDeviceSetupImpl::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  std::vector<multidevice::RemoteDevice> eligible_remote_devices;
  for (const auto& remote_device_ref :
       eligible_host_devices_provider_->GetEligibleHostDevices()) {
    eligible_remote_devices.push_back(remote_device_ref.GetRemoteDevice());
  }

  std::move(callback).Run(eligible_remote_devices);
}

void MultiDeviceSetupImpl::GetEligibleActiveHostDevices(
    GetEligibleActiveHostDevicesCallback callback) {
  // For metrics.
  bool has_duplicate_host_name = false;
  base::flat_set<std::string> name_set;

  std::vector<mojom::HostDevicePtr> eligible_active_hosts;
  for (const auto& host_device :
       eligible_host_devices_provider_->GetEligibleActiveHostDevices()) {
    // For metrics.
    if (base::Contains(name_set, host_device.remote_device.name())) {
      has_duplicate_host_name = true;
      PA_LOG(WARNING) << "MultiDeviceSetupImpl::GetEligibleActiveHostDevices: "
                      << "Detected duplicate eligible host device name \""
                      << host_device.remote_device.name() << "\"";
    } else {
      name_set.insert(host_device.remote_device.name());
    }

    eligible_active_hosts.push_back(
        mojom::HostDevice::New(host_device.remote_device.GetRemoteDevice(),
                               host_device.connectivity_status));
  }

  base::UmaHistogramBoolean(
      "MultiDevice.Setup.HasDuplicateEligibleHostDeviceNames",
      has_duplicate_host_name);
  base::UmaHistogramBoolean("MultiDevice.Setup.EligibleHostDeviceListCount",
                            eligible_active_hosts.size());

  std::move(callback).Run(std::move(eligible_active_hosts));
}

void MultiDeviceSetupImpl::SetHostDevice(
    const std::string& host_instance_id_or_legacy_device_id,
    const std::string& auth_token,
    SetHostDeviceCallback callback) {
  if (!auth_token_validator_->IsAuthTokenValid(auth_token)) {
    PA_LOG(WARNING) << "MultiDeviceSetupImpl::SetHostDevice failed due to "
                       "invalid auth token";
    std::move(callback).Run(false /* success */);
    return;
  }

  std::move(callback).Run(AttemptSetHost(host_instance_id_or_legacy_device_id));
}

void MultiDeviceSetupImpl::RemoveHostDevice() {
  LogForgetHostConfirmed(
      VerifyAndForgetHostConfirmationState::kButtonClickedState);

  host_backend_delegate_->AttemptToSetMultiDeviceHostOnBackend(
      std::nullopt /* host_device */);
}

void MultiDeviceSetupImpl::GetHostStatus(GetHostStatusCallback callback) {
  HostStatusProvider::HostStatusWithDevice host_status_with_device =
      host_status_provider_->GetHostWithStatus();

  // The Mojo API requires a raw multidevice::RemoteDevice instead of a
  // multidevice::RemoteDeviceRef.
  std::optional<multidevice::RemoteDevice> device_for_callback;
  if (host_status_with_device.host_device()) {
    device_for_callback =
        host_status_with_device.host_device()->GetRemoteDevice();
  }

  std::move(callback).Run(host_status_with_device.host_status(),
                          device_for_callback);
}

void MultiDeviceSetupImpl::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    const std::optional<std::string>& auth_token,
    SetFeatureEnabledStateCallback callback) {
  if (IsAuthTokenRequiredForFeatureStateChange(feature, enabled) &&
      (!auth_token || !auth_token_validator_->IsAuthTokenValid(*auth_token))) {
    PA_LOG(ERROR) << __func__ << " Cannot " << (enabled ? "enable" : "disable")
                  << " " << feature << "; auth token invalid";
    std::move(callback).Run(false /* success */);
    return;
  }

  std::move(callback).Run(
      feature_state_manager_->SetFeatureEnabledState(feature, enabled));
}

void MultiDeviceSetupImpl::GetFeatureStates(GetFeatureStatesCallback callback) {
  std::move(callback).Run(feature_state_manager_->GetFeatureStates());
}

void MultiDeviceSetupImpl::RetrySetHostNow(RetrySetHostNowCallback callback) {
  LogVerifyButtonClicked(
      VerifyAndForgetHostConfirmationState::kButtonClickedState);

  HostStatusProvider::HostStatusWithDevice host_status_with_device =
      host_status_provider_->GetHostWithStatus();

  if (host_status_with_device.host_status() ==
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation) {
    host_backend_delegate_->AttemptToSetMultiDeviceHostOnBackend(
        *host_backend_delegate_->GetPendingHostRequest());
    std::move(callback).Run(true /* success */);
    return;
  }

  if (host_status_with_device.host_status() ==
      mojom::HostStatus::kHostSetButNotYetVerified) {
    host_verifier_->AttemptVerificationNow();
    std::move(callback).Run(true /* success */);
    return;
  }

  // RetrySetHostNow() was called when there was nothing to retry.
  std::move(callback).Run(false /* success */);
}

void MultiDeviceSetupImpl::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  if (!delegate_notifier_->delegate_remote_) {
    PA_LOG(ERROR) << "MultiDeviceSetupImpl::TriggerEventForDebugging(): No "
                  << "delgate has been set; cannot proceed.";
    std::move(callback).Run(false /* success */);
    return;
  }

  PA_LOG(VERBOSE) << "MultiDeviceSetupImpl::TriggerEventForDebugging(" << type
                  << ") called.";
  mojom::AccountStatusChangeDelegate* delegate =
      delegate_notifier_->delegate_remote_.get();

  switch (type) {
    case mojom::EventTypeForDebugging::kNewUserPotentialHostExists:
      delegate->OnPotentialHostExistsForNewUser();
      break;
    case mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched:
      delegate->OnConnectedHostSwitchedForExistingUser(
          kTestDeviceNameForDebugNotification);
      break;
    case mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded:
      delegate->OnNewChromebookAddedForExistingUser(
          kTestDeviceNameForDebugNotification);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  std::move(callback).Run(true /* success */);
}

void MultiDeviceSetupImpl::SetQuickStartPhoneInstanceID(
    const std::string& qs_phone_instance_id) {
  qs_phone_instance_id_ = qs_phone_instance_id;
}

void MultiDeviceSetupImpl::GetQuickStartPhoneInstanceID(
    GetQuickStartPhoneInstanceIDCallback callback) {
  if (qs_phone_instance_id_.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(qs_phone_instance_id_);
}

void MultiDeviceSetupImpl::SetHostDeviceWithoutAuthToken(
    const std::string& host_instance_id_or_legacy_device_id,
    mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback) {
  std::move(callback).Run(AttemptSetHost(host_instance_id_or_legacy_device_id));
}

void MultiDeviceSetupImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  mojom::HostStatus status_for_callback = host_status_with_device.host_status();

  // The Mojo API requires a raw multidevice::RemoteDevice instead of a
  // multidevice::RemoteDeviceRef.
  std::optional<multidevice::RemoteDevice> device_for_callback;
  if (host_status_with_device.host_device()) {
    device_for_callback =
        host_status_with_device.host_device()->GetRemoteDevice();
  }

  for (auto& observer : host_status_observers_)
    observer->OnHostStatusChanged(status_for_callback, device_for_callback);
}

void MultiDeviceSetupImpl::OnFeatureStatesChange(
    const FeatureStateManager::FeatureStatesMap& feature_states_map) {
  for (auto& observer : feature_state_observers_)
    observer->OnFeatureStatesChanged(feature_states_map);
}

bool MultiDeviceSetupImpl::AttemptSetHost(
    const std::string& host_instance_id_or_legacy_device_id) {
  DCHECK(!host_instance_id_or_legacy_device_id.empty());

  multidevice::RemoteDeviceRefList eligible_devices =
      eligible_host_devices_provider_->GetEligibleHostDevices();
  if (eligible_devices.empty()) {
    PA_LOG(WARNING)
        << __func__
        << ": attempting to set host but no eligible devices are available";
  }

  auto it = base::ranges::find_if(
      eligible_devices,
      [&host_instance_id_or_legacy_device_id](const auto& eligible_device) {
        if (features::ShouldUseV1DeviceSync()) {
          return eligible_device.instance_id() ==
                     host_instance_id_or_legacy_device_id ||
                 eligible_device.GetDeviceId() ==
                     host_instance_id_or_legacy_device_id;
        }

        return eligible_device.instance_id() ==
               host_instance_id_or_legacy_device_id;
      });

  if (it == eligible_devices.end()) {
    PA_LOG(WARNING)
        << " MultiDeviceSetupImpl::AttemptSetHost failed because there was no "
           "match in the eligible devices for the selected host";
    return false;
  }

  LogForgetHostConfirmed(
      VerifyAndForgetHostConfirmationState::kCompletedSetupState);

  LogVerifyButtonClicked(
      VerifyAndForgetHostConfirmationState::kCompletedSetupState);

  host_backend_delegate_->AttemptToSetMultiDeviceHostOnBackend(*it);

  return true;
}

bool MultiDeviceSetupImpl::IsAuthTokenRequiredForFeatureStateChange(
    mojom::Feature feature,
    bool enabled) {
  // Disabling a feature never requires authentication.
  if (!enabled)
    return false;

  // Enabling SmartLock always requires authentication.
  if (feature == mojom::Feature::kSmartLock)
    return true;

  // Enabling any feature besides SmartLock and the Better Together suite does
  // not require authentication.
  if (feature != mojom::Feature::kBetterTogetherSuite)
    return false;

  mojom::FeatureState smart_lock_state =
      feature_state_manager_->GetFeatureStates()[mojom::Feature::kSmartLock];

  // If the user is enabling the Better Together suite and this change would
  // result in SmartLock being implicitly enabled, authentication is required.
  // SmartLock is implicitly enabled if it is only currently not enabled due
  // to the suite being disabled or due to the SmartLock host device not
  // having a lock screen set.
  return smart_lock_state == mojom::FeatureState::kUnavailableSuiteDisabled ||
         smart_lock_state ==
             mojom::FeatureState::kUnavailableInsufficientSecurity;
}

void MultiDeviceSetupImpl::FlushForTesting() {
  host_status_observers_.FlushForTesting();
  feature_state_observers_.FlushForTesting();
}

}  // namespace multidevice_setup

}  // namespace ash
