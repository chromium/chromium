// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/android_sms_app_installing_status_observer.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
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
    AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate) {
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
    AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate)
    : host_status_provider_(host_status_provider),
      feature_state_manager_(feature_state_manager),
      android_sms_app_helper_delegate_(android_sms_app_helper_delegate) {
  host_status_provider_->AddObserver(this);
  feature_state_manager_->AddObserver(this);
  UpdatePwaInstallationState();
}

bool AndroidSmsAppInstallingStatusObserver::
    DoesFeatureStateAllowInstallation() {
  mojom::FeatureState feature_state =
      feature_state_manager_->GetFeatureStates()[mojom::Feature::kMessages];
  if (feature_state != mojom::FeatureState::kEnabledByUser &&
      feature_state != mojom::FeatureState::kFurtherSetupRequired) {
    return false;
  }

  mojom::HostStatus status(
      host_status_provider_->GetHostWithStatus().host_status());
  if (status !=
          mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation &&
      status != mojom::HostStatus::kHostVerified) {
    return false;
  }

  return true;
}

void AndroidSmsAppInstallingStatusObserver::UpdatePwaInstallationState() {
  if (!DoesFeatureStateAllowInstallation()) {
    // The feature is disabled, ensure that the integration cookie is removed.
    android_sms_app_helper_delegate_->TearDownAndroidSmsApp();
    return;
  }

  if (android_sms_app_helper_delegate_->HasAppBeenManuallyUninstalledByUser()) {
    feature_state_manager_->SetFeatureEnabledState(mojom::Feature::kMessages,
                                                   false);

    // The feature is now disabled, clear the cookie and pref.
    android_sms_app_helper_delegate_->TearDownAndroidSmsApp();

    return;
  }

  // Otherwise, set the default to persist cookie and install the PWA.
  android_sms_app_helper_delegate_->SetUpAndroidSmsApp();
}

void AndroidSmsAppInstallingStatusObserver::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  UpdatePwaInstallationState();
}

void AndroidSmsAppInstallingStatusObserver::OnFeatureStatesChange(
    const FeatureStateManager::FeatureStatesMap& feature_states_map) {
  UpdatePwaInstallationState();
}

}  // namespace multidevice_setup

}  // namespace chromeos
