// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/android_sms_app_installing_status_observer.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace multidevice_setup {

namespace {

const char kShouldAttemptReenable[] = "android_sms.should_attempt_reenable";

}  // namespace

// static
AndroidSmsAppInstallingStatusObserver::Factory*
    AndroidSmsAppInstallingStatusObserver::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
AndroidSmsAppInstallingStatusObserver::Factory::Create(
    HostStatusProvider* host_status_provider,
    FeatureStateManager* feature_state_manager,
    AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
    PrefService* pref_service) {
  if (test_factory_) {
    return test_factory_->CreateInstance(host_status_provider,
                                         feature_state_manager,
                                         android_sms_app_helper_delegate);
  }
  return base::WrapUnique(new AndroidSmsAppInstallingStatusObserver(
      host_status_provider, feature_state_manager,
      android_sms_app_helper_delegate, pref_service));
}

// static
void AndroidSmsAppInstallingStatusObserver::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

AndroidSmsAppInstallingStatusObserver::Factory::~Factory() = default;

AndroidSmsAppInstallingStatusObserver::
    ~AndroidSmsAppInstallingStatusObserver() {
  host_status_provider_->RemoveObserver(this);
  feature_state_manager_->RemoveObserver(this);
}

// static
void AndroidSmsAppInstallingStatusObserver::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kShouldAttemptReenable, true);
}

AndroidSmsAppInstallingStatusObserver::AndroidSmsAppInstallingStatusObserver(
    HostStatusProvider* host_status_provider,
    FeatureStateManager* feature_state_manager,
    AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
    PrefService* pref_service)
    : host_status_provider_(host_status_provider),
      feature_state_manager_(feature_state_manager),
      android_sms_app_helper_delegate_(android_sms_app_helper_delegate),
      pref_service_(pref_service) {
  host_status_provider_->AddObserver(this);
  feature_state_manager_->AddObserver(this);

  // Wait until the app registry has been loaded before updating installation
  // status.
  android_sms_app_helper_delegate_->ExecuteOnAppRegistryReady(base::BindOnce(
      &AndroidSmsAppInstallingStatusObserver::UpdatePwaInstallationState,
      weak_ptr_factory_.GetWeakPtr()));
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

void AndroidSmsAppInstallingStatusObserver::ReenableIfAppropriate() {
  if (!pref_service_->GetBoolean(kShouldAttemptReenable)) {
    return;
  }

  // This is a one-time attempt, flip the pref to prevent later tries.
  pref_service_->SetBoolean(kShouldAttemptReenable, false);

  if (host_status_provider_->GetHostWithStatus().host_status() !=
      mojom::HostStatus::kHostVerified) {
    PA_LOG(INFO) << "Can't reenable Messages, no verified host.";
    return;
  }

  if (feature_state_manager_->GetFeatureStates()[mojom::Feature::kMessages] !=
      mojom::FeatureState::kDisabledByUser) {
    PA_LOG(INFO)
        << "Can't reenable Messages, feature is not in disabled state.";
    return;
  }

  if (!android_sms_app_helper_delegate_->IsAppInstalled()) {
    PA_LOG(INFO) << "Can't reenable Messages, app not installed.";
    return;
  }

  PA_LOG(INFO) << "Performing one-time re-enable.";
  feature_state_manager_->SetFeatureEnabledState(mojom::Feature::kMessages,
                                                 true);
}

void AndroidSmsAppInstallingStatusObserver::UpdatePwaInstallationState() {
  if (!android_sms_app_helper_delegate_->IsAppRegistryReady()) {
    PA_LOG(INFO) << "App registry is not ready.";
    return;
  }

  // TODO(crbug/1131140): Remove in M-89.  This is needed to correct a bug
  // introduced in M-85 and is not permanently.
  ReenableIfAppropriate();

  if (!DoesFeatureStateAllowInstallation()) {
    PA_LOG(INFO)
        << "Feature state does not allow installation, tearing down App.";
    // The feature is disabled, ensure that the integration cookie is removed.
    android_sms_app_helper_delegate_->TearDownAndroidSmsApp();
    return;
  }

  if (android_sms_app_helper_delegate_->HasAppBeenManuallyUninstalledByUser()) {
    PA_LOG(INFO) << "App was manually uninstalled by user, tearing down App.";
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

}  // namespace ash
