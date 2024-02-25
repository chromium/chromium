// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/onboarding_ui_tracker_impl.h"

#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace phonehub {

void OnboardingUiTrackerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kHideOnboardingUi, false);
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
  multidevice_setup_client_->AddObserver(this);

  should_show_onboarding_ui_ = ComputeShouldShowOnboardingUi();
}

OnboardingUiTrackerImpl::~OnboardingUiTrackerImpl() {
  feature_status_provider_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

bool OnboardingUiTrackerImpl::ShouldShowOnboardingUi() const {
  return should_show_onboarding_ui_;
}

void OnboardingUiTrackerImpl::DismissSetupUi() {
  pref_service_->SetBoolean(prefs::kHideOnboardingUi, true);
  UpdateShouldShowOnboardingUi();
}

void OnboardingUiTrackerImpl::HandleGetStarted(
    bool is_icon_clicked_when_nudge_visible) {
  FeatureStatus status = feature_status_provider_->GetStatus();

  // The user is not opted into Better Together yet.
  if (status == FeatureStatus::kEligiblePhoneButNotSetUp) {
    show_multidevice_setup_dialog_callback_.Run();
    if (is_icon_clicked_when_nudge_visible) {
      util::LogMultiDeviceSetupDialogEntryPoint(
          util::MultiDeviceSetupDialogEntrypoint::kPhoneHubBubbleAferNudge);
    } else {
      util::LogMultiDeviceSetupDialogEntryPoint(
          util::MultiDeviceSetupDialogEntrypoint::kPhoneHubBubble);
    }
    return;
  }

  // The user is already opted into Better Together, but not Phone Hub.
  if (status == FeatureStatus::kDisabled) {
    multidevice_setup_client_->SetFeatureEnabledState(
        multidevice_setup::mojom::Feature::kPhoneHub,
        /*enabled=*/true, /*auth_token=*/std::nullopt, base::DoNothing());
    util::LogFeatureOptInEntryPoint(util::OptInEntryPoint::kOnboardingFlow);
    return;
  }
  LOG(ERROR)
      << "Cannot handle a GetStarted request because the current state is "
      << status;
}

void OnboardingUiTrackerImpl::OnFeatureStatusChanged() {
  UpdateShouldShowOnboardingUi();
}

void OnboardingUiTrackerImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  const multidevice_setup::mojom::FeatureState phonehub_state =
      feature_states_map.find(multidevice_setup::mojom::Feature::kPhoneHub)
          ->second;
  // User has gone through the onboarding process, prevent the UI from
  // displaying again.
  if (phonehub_state ==
      multidevice_setup::mojom::FeatureState::kEnabledByUser) {
    pref_service_->SetBoolean(prefs::kHideOnboardingUi, true);
    UpdateShouldShowOnboardingUi();
  }
}

bool OnboardingUiTrackerImpl::ComputeShouldShowOnboardingUi() {
  FeatureStatus status = feature_status_provider_->GetStatus();

  if (status == FeatureStatus::kEligiblePhoneButNotSetUp ||
      status == FeatureStatus::kDisabled) {
    return !pref_service_->GetBoolean(prefs::kHideOnboardingUi);
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
}  // namespace ash
