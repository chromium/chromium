// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/onboarding_ui_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace phonehub {

// OnboardingUiTracker implementation that uses the
// |kHideOnboardingUi| pref to determine whether the Onboarding UI should be
// shown. This class invokes |show_multidevice_setup_dialog_callback| when the
// user proceeds with the onboarding flow if Better Together is disabled. If
// Better Together is enabled, but PhoneHub is disabled, this class enables the
// PhoneHub feature via the MultiDeviceSetupClient instead.
class OnboardingUiTrackerImpl
    : public OnboardingUiTracker,
      public FeatureStatusProvider::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  OnboardingUiTrackerImpl(
      PrefService* pref_service,
      FeatureStatusProvider* feature_status_provider,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      const base::RepeatingClosure& show_multidevice_setup_dialog_callback);
  ~OnboardingUiTrackerImpl() override;

  // OnboardingUiTracker:
  bool ShouldShowOnboardingUi() const override;
  void DismissSetupUi() override;
  void HandleGetStarted(bool is_icon_clicked_when_nudge_visible) override;

 private:
  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  bool ComputeShouldShowOnboardingUi();
  void UpdateShouldShowOnboardingUi();
  raw_ptr<PrefService> pref_service_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  bool should_show_onboarding_ui_;
  base::RepeatingClosure show_multidevice_setup_dialog_callback_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_IMPL_H_
