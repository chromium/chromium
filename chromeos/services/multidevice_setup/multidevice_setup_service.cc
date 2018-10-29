// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"

#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/services/multidevice_setup/device_reenroller.h"
#include "chromeos/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_device_timestamp_manager_impl.h"
#include "chromeos/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_initializer.h"
#include "chromeos/services/multidevice_setup/privileged_host_device_setter_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"

namespace chromeos {

namespace multidevice_setup {

// static
void MultiDeviceSetupService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  HostDeviceTimestampManagerImpl::RegisterPrefs(registry);
  AccountStatusChangeDelegateNotifierImpl::RegisterPrefs(registry);
  HostBackendDelegateImpl::RegisterPrefs(registry);
  HostVerifierImpl::RegisterPrefs(registry);
  GrandfatheredEasyUnlockHostDisabler::RegisterPrefs(registry);
  RegisterFeaturePrefs(registry);
}

MultiDeviceSetupService::MultiDeviceSetupService(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AuthTokenValidator* auth_token_validator,
    OobeCompletionTracker* oobe_completion_tracker,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate,
    std::unique_ptr<AndroidSmsPairingStateTracker>
        android_sms_pairing_state_tracker,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider)
    : multidevice_setup_(
          MultiDeviceSetupInitializer::Factory::Get()->BuildInstance(
              pref_service,
              device_sync_client,
              auth_token_validator,
              oobe_completion_tracker,
              std::move(android_sms_app_helper_delegate),
              std::move(android_sms_pairing_state_tracker),
              gcm_device_info_provider)),
      privileged_host_device_setter_(
          PrivilegedHostDeviceSetterImpl::Factory::Get()->BuildInstance(
              multidevice_setup_.get())) {}

MultiDeviceSetupService::~MultiDeviceSetupService() = default;

void MultiDeviceSetupService::OnStart() {
  PA_LOG(INFO) << "MultiDeviceSetupService::OnStart()";
  registry_.AddInterface(
      base::BindRepeating(&MultiDeviceSetupBase::BindRequest,
                          base::Unretained(multidevice_setup_.get())));
  registry_.AddInterface(base::BindRepeating(
      &PrivilegedHostDeviceSetterBase::BindRequest,
      base::Unretained(privileged_host_device_setter_.get())));
}

void MultiDeviceSetupService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  PA_LOG(INFO) << "MultiDeviceSetupService::OnBindInterface() from interface "
               << interface_name << ".";
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace multidevice_setup

}  // namespace chromeos
