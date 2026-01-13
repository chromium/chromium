// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/android_parental_controls.h"

#include <string>

#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"

namespace supervised_user {
namespace {
const char kDeviceSearchContentFiltersSyntheticFieldTrialName[] =
    "AndroidDeviceSearchContentFilters";
const char kDeviceBrowserContentFiltersSyntheticFieldTrialName[] =
    "AndroidDeviceBrowserContentFilters";
std::string GetDeviceFiltersSynthenticFieldTrialGroupName(bool filter_enabled) {
  return filter_enabled ? "Enabled" : "Disabled";
}
}  // namespace

AndroidParentalControls::AndroidParentalControls() {
  browser_content_filters_observation_.Observe(
      &browser_content_filters_observer_);
  search_content_filters_observation_.Observe(
      &search_content_filters_observer_);
}

void AndroidParentalControls::Init() {
  // TODO(crbug.com/471178506): initialize the bridges lazily.
  browser_content_filters_observer_.Init();
  search_content_filters_observer_.Init();
}

AndroidParentalControls::~AndroidParentalControls() = default;

bool AndroidParentalControls::IsWebFilteringEnabled() const {
  return IsBrowserContentFiltersEnabled();
}

bool AndroidParentalControls::IsIncognitoModeDisabled() const {
  return IsBrowserContentFiltersEnabled() || IsSearchContentFiltersEnabled();
}

bool AndroidParentalControls::IsSafeSearchForced() const {
  return IsSearchContentFiltersEnabled();
}

bool AndroidParentalControls::IsEnabled() const {
  return IsBrowserContentFiltersEnabled() || IsSearchContentFiltersEnabled();
}

bool AndroidParentalControls::IsBrowserContentFiltersEnabled() const {
  return browser_content_filters_observer_.IsEnabled();
}

bool AndroidParentalControls::IsSearchContentFiltersEnabled() const {
  return search_content_filters_observer_.IsEnabled();
}

void AndroidParentalControls::OnContentFiltersObserverChanged() {
  NotifySubscribers();
}

void AndroidParentalControls::SetBrowserContentFiltersEnabledForTesting(
    bool enabled) {
  browser_content_filters_observer_.SetEnabledForTesting(enabled);
}

void AndroidParentalControls::SetSearchContentFiltersEnabledForTesting(
    bool enabled) {
  search_content_filters_observer_.SetEnabledForTesting(enabled);
}

void AndroidParentalControls::RegisterDeviceLevelSyntheticFieldTrials(
    SynteticFieldTrialDelegate& synthetic_field_trial_delegate) const {
  synthetic_field_trial_delegate.RegisterSyntheticFieldTrial(
      kDeviceBrowserContentFiltersSyntheticFieldTrialName,
      GetDeviceFiltersSynthenticFieldTrialGroupName(
          IsBrowserContentFiltersEnabled()));
  synthetic_field_trial_delegate.RegisterSyntheticFieldTrial(
      kDeviceSearchContentFiltersSyntheticFieldTrialName,
      GetDeviceFiltersSynthenticFieldTrialGroupName(
          IsSearchContentFiltersEnabled()));
}

bool AreAndroidParentalControlsEffectiveForTesting(
    const PrefService& pref_service) {
  if (IsSubjectToParentalControls(pref_service)) {
    return false;
  }

  // When any device parental controls are active, they disable incognito mode.
  // This is done by setting the `kIncognitoModeAvailability` pref, which
  // results in `IsManagedByCustodian()` returning true for that pref.
  // We use this as a proxy to determine if device controls are "effective".
  // This check is only reached if Family Link supervision is not active,
  // as determined by the `IsSubjectToParentalControls` check above.
  return pref_service
      .FindPreference(policy::policy_prefs::kIncognitoModeAvailability)
      ->IsManagedByCustodian();
}
}  // namespace supervised_user
