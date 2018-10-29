// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/default_clock.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/services/multidevice_setup/android_sms_app_installing_status_observer.h"
#include "chromeos/services/multidevice_setup/device_reenroller.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"
#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"
#include "chromeos/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_device_timestamp_manager_impl.h"
#include "chromeos/services/multidevice_setup/host_status_provider_impl.h"
#include "chromeos/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/auth_token_validator.h"
#include "chromeos/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"

namespace chromeos {

namespace multidevice_setup {

namespace {
const char kTestDeviceNameForDebugNotification[] = "Test Device";
}  // namespace

// static
MultiDeviceSetupImpl::Factory* MultiDeviceSetupImpl::Factory::test_factory_ =
    nullptr;

// static
MultiDeviceSetupImpl::Factory* MultiDeviceSetupImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void MultiDeviceSetupImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupImpl::Factory::~Factory() = default;

std::unique_ptr<MultiDeviceSetupBase>
MultiDeviceSetupImpl::Factory::BuildInstance(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AuthTokenValidator* auth_token_validator,
    OobeCompletionTracker* oobe_completion_tracker,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate,
    std::unique_ptr<AndroidSmsPairingStateTracker>
        android_sms_pairing_state_tracker,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider) {
  return base::WrapUnique(new MultiDeviceSetupImpl(
      pref_service, device_sync_client, auth_token_validator,
      oobe_completion_tracker, std::move(android_sms_app_helper_delegate),
      std::move(android_sms_pairing_state_tracker), gcm_device_info_provider));
}

MultiDeviceSetupImpl::MultiDeviceSetupImpl(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AuthTokenValidator* auth_token_validator,
    OobeCompletionTracker* oobe_completion_tracker,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate,
    std::unique_ptr<AndroidSmsPairingStateTracker>
        android_sms_pairing_state_tracker,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider)
    : eligible_host_devices_provider_(
          EligibleHostDevicesProviderImpl::Factory::Get()->BuildInstance(
              device_sync_client)),
      host_backend_delegate_(
          HostBackendDelegateImpl::Factory::Get()->BuildInstance(
              eligible_host_devices_provider_.get(),
              pref_service,
              device_sync_client)),
      host_verifier_(HostVerifierImpl::Factory::Get()->BuildInstance(
          host_backend_delegate_.get(),
          device_sync_client,
          pref_service)),
      host_status_provider_(
          HostStatusProviderImpl::Factory::Get()->BuildInstance(
              eligible_host_devices_provider_.get(),
              host_backend_delegate_.get(),
              host_verifier_.get(),
              device_sync_client)),
      grandfathered_easy_unlock_host_disabler_(
          GrandfatheredEasyUnlockHostDisabler::Factory::Get()->BuildInstance(
              host_backend_delegate_.get(),
              device_sync_client,
              pref_service)),
      feature_state_manager_(
          FeatureStateManagerImpl::Factory::Get()->BuildInstance(
              pref_service,
              host_status_provider_.get(),
              device_sync_client,
              std::move(android_sms_pairing_state_tracker))),
      host_device_timestamp_manager_(
          HostDeviceTimestampManagerImpl::Factory::Get()->BuildInstance(
              host_status_provider_.get(),
              pref_service,
              base::DefaultClock::GetInstance())),
      delegate_notifier_(
          AccountStatusChangeDelegateNotifierImpl::Factory::Get()
              ->BuildInstance(host_status_provider_.get(),
                              pref_service,
                              host_device_timestamp_manager_.get(),
                              oobe_completion_tracker,
                              base::DefaultClock::GetInstance())),
      device_reenroller_(DeviceReenroller::Factory::Get()->BuildInstance(
          device_sync_client,
          gcm_device_info_provider)),
      android_sms_app_installing_host_observer_(
          AndroidSmsAppInstallingStatusObserver::Factory::Get()->BuildInstance(
              host_status_provider_.get(),
              feature_state_manager_.get(),
              std::move(android_sms_app_helper_delegate))),
      auth_token_validator_(auth_token_validator) {
  host_status_provider_->AddObserver(this);
  feature_state_manager_->AddObserver(this);
}

MultiDeviceSetupImpl::~MultiDeviceSetupImpl() {
  host_status_provider_->RemoveObserver(this);
  feature_state_manager_->RemoveObserver(this);
}

void MultiDeviceSetupImpl::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate) {
  delegate_notifier_->SetAccountStatusChangeDelegatePtr(std::move(delegate));
}

void MultiDeviceSetupImpl::AddHostStatusObserver(
    mojom::HostStatusObserverPtr observer) {
  host_status_observers_.AddPtr(std::move(observer));
}

void MultiDeviceSetupImpl::AddFeatureStateObserver(
    mojom::FeatureStateObserverPtr observer) {
  feature_state_observers_.AddPtr(std::move(observer));
}

void MultiDeviceSetupImpl::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  std::vector<cryptauth::RemoteDevice> eligible_remote_devices;
  for (const auto& remote_device_ref :
       eligible_host_devices_provider_->GetEligibleHostDevices()) {
    eligible_remote_devices.push_back(remote_device_ref.GetRemoteDevice());
  }

  // Sort from most-recently-updated to least-recently-updated. The timestamp
  // used is provided by the back-end and indicates the last time at which the
  // device's metadata was updated on the server. Note that this does not
  // provide us with the last time that a user actually used this device, but it
  // is a good estimate.
  std::sort(eligible_remote_devices.begin(), eligible_remote_devices.end(),
            [](const auto& first_device, const auto& second_device) {
              return first_device.last_update_time_millis >
                     second_device.last_update_time_millis;
            });

  std::move(callback).Run(eligible_remote_devices);
}

void MultiDeviceSetupImpl::SetHostDevice(const std::string& host_device_id,
                                         const std::string& auth_token,
                                         SetHostDeviceCallback callback) {
  if (!auth_token_validator_->IsAuthTokenValid(auth_token)) {
    std::move(callback).Run(false /* success */);
    return;
  }

  std::move(callback).Run(AttemptSetHost(host_device_id));
}

void MultiDeviceSetupImpl::RemoveHostDevice() {
  host_backend_delegate_->AttemptToSetMultiDeviceHostOnBackend(
      base::nullopt /* host_device */);
}

void MultiDeviceSetupImpl::GetHostStatus(GetHostStatusCallback callback) {
  HostStatusProvider::HostStatusWithDevice host_status_with_device =
      host_status_provider_->GetHostWithStatus();

  // The Mojo API requires a raw cryptauth::RemoteDevice instead of a
  // cryptauth::RemoteDeviceRef.
  base::Optional<cryptauth::RemoteDevice> device_for_callback;
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
    const base::Optional<std::string>& auth_token,
    SetFeatureEnabledStateCallback callback) {
  if (IsAuthTokenRequiredForFeatureStateChange(feature, enabled) &&
      (!auth_token || !auth_token_validator_->IsAuthTokenValid(*auth_token))) {
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
  if (!delegate_notifier_->delegate_ptr_) {
    PA_LOG(ERROR) << "MultiDeviceSetupImpl::TriggerEventForDebugging(): No "
                  << "delgate has been set; cannot proceed.";
    std::move(callback).Run(false /* success */);
    return;
  }

  PA_LOG(INFO) << "MultiDeviceSetupImpl::TriggerEventForDebugging(" << type
               << ") called.";
  mojom::AccountStatusChangeDelegate* delegate =
      delegate_notifier_->delegate_ptr_.get();

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
      NOTREACHED();
  }

  std::move(callback).Run(true /* success */);
}

void MultiDeviceSetupImpl::SetHostDeviceWithoutAuthToken(
    const std::string& host_device_id,
    mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback) {
  std::move(callback).Run(AttemptSetHost(host_device_id));
}

void MultiDeviceSetupImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  mojom::HostStatus status_for_callback = host_status_with_device.host_status();

  // The Mojo API requires a raw cryptauth::RemoteDevice instead of a
  // cryptauth::RemoteDeviceRef.
  base::Optional<cryptauth::RemoteDevice> device_for_callback;
  if (host_status_with_device.host_device()) {
    device_for_callback =
        host_status_with_device.host_device()->GetRemoteDevice();
  }

  host_status_observers_.ForAllPtrs(
      [&status_for_callback,
       &device_for_callback](mojom::HostStatusObserver* observer) {
        observer->OnHostStatusChanged(status_for_callback, device_for_callback);
      });
}

void MultiDeviceSetupImpl::OnFeatureStatesChange(
    const FeatureStateManager::FeatureStatesMap& feature_states_map) {
  feature_state_observers_.ForAllPtrs(
      [&feature_states_map](mojom::FeatureStateObserver* observer) {
        observer->OnFeatureStatesChanged(feature_states_map);
      });
}

bool MultiDeviceSetupImpl::AttemptSetHost(const std::string& host_device_id) {
  cryptauth::RemoteDeviceRefList eligible_devices =
      eligible_host_devices_provider_->GetEligibleHostDevices();
  auto it =
      std::find_if(eligible_devices.begin(), eligible_devices.end(),
                   [&host_device_id](const auto& eligible_device) {
                     return eligible_device.GetDeviceId() == host_device_id;
                   });

  if (it == eligible_devices.end())
    return false;

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

}  // namespace chromeos
