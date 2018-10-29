// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/android_sms_app_installing_status_observer.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// static
AndroidSmsAppInstallingStatusObserver::Factory*
    AndroidSmsAppInstallingStatusObserver::Factory::test_factory_ = nullptr;

// static
AndroidSmsAppInstallingStatusObserver::Factory*
AndroidSmsAppInstallingStatusObserver::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void AndroidSmsAppInstallingStatusObserver::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

AndroidSmsAppInstallingStatusObserver::Factory::~Factory() = default;

std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
AndroidSmsAppInstallingStatusObserver::Factory::BuildInstance(
    HostStatusProvider* host_status_provider,
    FeatureStateManager* feature_state_manager,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate) {
  return base::WrapUnique(new AndroidSmsAppInstallingStatusObserver(
      host_status_provider, feature_state_manager,
      std::move(android_sms_app_helper_delegate)));
}

AndroidSmsAppInstallingStatusObserver::
    ~AndroidSmsAppInstallingStatusObserver() {
  host_status_provider_->RemoveObserver(this);
  feature_state_manager_->RemoveObserver(this);
}

AndroidSmsAppInstallingStatusObserver::AndroidSmsAppInstallingStatusObserver(
    HostStatusProvider* host_status_provider,
    FeatureStateManager* feature_state_manager,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate)
    : host_status_provider_(host_status_provider),
      feature_state_manager_(feature_state_manager),
      android_sms_app_helper_delegate_(
          std::move(android_sms_app_helper_delegate)) {
  host_status_provider_->AddObserver(this);
  feature_state_manager_->AddObserver(this);
  InstallPwaIfNeeded();
}

void AndroidSmsAppInstallingStatusObserver::InstallPwaIfNeeded() {
  mojom::FeatureState feature_state =
      feature_state_manager_->GetFeatureStates()[mojom::Feature::kMessages];
  if (feature_state == mojom::FeatureState::kProhibitedByPolicy ||
      feature_state == mojom::FeatureState::kNotSupportedByChromebook ||
      feature_state == mojom::FeatureState::kNotSupportedByPhone) {
    return;
  }

  mojom::HostStatus status(
      host_status_provider_->GetHostWithStatus().host_status());
  if (status !=
          mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation &&
      status != mojom::HostStatus::kHostVerified) {
    return;
  }

  // This call is re-entrant. If the app is already installed, it will just
  // fail silently, which is fine.
  android_sms_app_helper_delegate_->InstallAndroidSmsApp();
}

void AndroidSmsAppInstallingStatusObserver::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  InstallPwaIfNeeded();
}

void AndroidSmsAppInstallingStatusObserver::OnFeatureStatesChange(
    const FeatureStateManager::FeatureStatesMap& feature_states_map) {
  InstallPwaIfNeeded();
}

}  // namespace multidevice_setup

}  // namespace chromeos
