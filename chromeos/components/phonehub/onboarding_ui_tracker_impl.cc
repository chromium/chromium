// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/onboarding_ui_tracker_impl.h"
#include "chromeos/components/phonehub/feature_status.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace phonehub {

void OnboardingUiTrackerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kHasDismissedUiAfterCompletingOnboarding,
                                false);
}

OnboardingUiTrackerImpl::OnboardingUiTrackerImpl(
    PrefService* pref_service,
    FeatureStatusProvider* feature_status_provider,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    const base::RepeatingClosure& show_multidevice_setup_dialog_callback)
    : pref_service_(pref_service),
      feature_status_provider_(feature_status_provider),
      multidevice_setup_client_(multidevice_setup_client),
      show_multidevice_setup_dialog_callback_(
          std::move(show_multidevice_setup_dialog_callback)) {
  feature_status_provider_->AddObserver(this);
  should_show_onboarding_ui_ = ComputeShouldShowOnboardingUi();
}

OnboardingUiTrackerImpl::~OnboardingUiTrackerImpl() {
  feature_status_provider_->RemoveObserver(this);
}

bool OnboardingUiTrackerImpl::ShouldShowOnboardingUi() const {
  return should_show_onboarding_ui_;
}

void OnboardingUiTrackerImpl::DismissSetupUi() {
  pref_service_->SetBoolean(prefs::kHasDismissedUiAfterCompletingOnboarding,
                            true);
  UpdateShouldShowOnboardingUi();
}

void OnboardingUiTrackerImpl::HandleGetStarted() {
  FeatureStatus status = feature_status_provider_->GetStatus();

  // The user is not opted into Better Together yet.
  if (status == FeatureStatus::kEligiblePhoneButNotSetUp) {
    show_multidevice_setup_dialog_callback_.Run();
    return;
  }

  // The user is already opted into Better Together, but not Phone Hub.
  if (status == FeatureStatus::kDisabled) {
    multidevice_setup_client_->SetFeatureEnabledState(
        multidevice_setup::mojom::Feature::kPhoneHub,
        /*enabled=*/true, /*auth_token=*/base::nullopt, base::DoNothing());
    return;
  }
  LOG(ERROR)
      << "Cannot handle a GetStarted request because the current state is "
      << status;
}

void OnboardingUiTrackerImpl::OnFeatureStatusChanged() {
  UpdateShouldShowOnboardingUi();
}

bool OnboardingUiTrackerImpl::ComputeShouldShowOnboardingUi() {
  FeatureStatus status = feature_status_provider_->GetStatus();
  if (status == FeatureStatus::kEligiblePhoneButNotSetUp ||
      status == FeatureStatus::kDisabled) {
    return !pref_service_->GetBoolean(
        prefs::kHasDismissedUiAfterCompletingOnboarding);
  }
  return false;
}

void OnboardingUiTrackerImpl::UpdateShouldShowOnboardingUi() {
  bool should_show_onboarding_ui = ComputeShouldShowOnboardingUi();
  if (should_show_onboarding_ui_ == should_show_onboarding_ui)
    return;
  should_show_onboarding_ui_ = should_show_onboarding_ui;
  NotifyShouldShowOnboardingUiChanged();
}

}  // namespace phonehub
}  // namespace chromeos
