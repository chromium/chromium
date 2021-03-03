// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_picker.h"

#include <algorithm>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr base::TimeDelta kActiveTimeThreshold = base::TimeDelta::FromDays(28);

ProfilePicker::AvailabilityOnStartup GetAvailabilityOnStartup() {
  int availability_on_startup = g_browser_process->local_state()->GetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup);
  switch (availability_on_startup) {
    case 0:
      return ProfilePicker::AvailabilityOnStartup::kEnabled;
    case 1:
      return ProfilePicker::AvailabilityOnStartup::kDisabled;
    case 2:
      return ProfilePicker::AvailabilityOnStartup::kForced;
    default:
      NOTREACHED();
  }
  return ProfilePicker::AvailabilityOnStartup::kEnabled;
}

}  // namespace

const char ProfilePicker::kTaskManagerUrl[] =
    "chrome://profile-picker/task-manager";

const base::Feature kEnableProfilePickerOnStartupFeature{
    "EnableProfilePickerOnStartup", base::FEATURE_ENABLED_BY_DEFAULT};

// static
bool ProfilePicker::Shown() {
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  return prefs->GetBoolean(prefs::kBrowserProfilePickerShown);
}

// static
bool ProfilePicker::ShouldShowAtLaunch() {
  AvailabilityOnStartup availability_on_startup = GetAvailabilityOnStartup();

  if (!base::FeatureList::IsEnabled(features::kNewProfilePicker))
    return false;

  if (!base::FeatureList::IsEnabled(kEnableProfilePickerOnStartupFeature))
    return false;

  if (availability_on_startup == AvailabilityOnStartup::kDisabled)
    return false;

  // TODO (crbug/1155158): Move this over the urls check (in
  // startup_browser_creator.cc) once the profile picker can forward urls
  // specified in command line.
  if (availability_on_startup == AvailabilityOnStartup::kForced)
    return true;

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  size_t number_of_profiles = profile_manager->GetNumberOfProfiles();
  // Need to consider 0 profiles as this is what happens in some browser-tests.
  if (number_of_profiles <= 1)
    return false;

  std::vector<ProfileAttributesEntry*> profile_attributes =
      profile_manager->GetProfileAttributesStorage().GetAllProfilesAttributes(
          /*include_guest_profile=*/false);
  int number_of_active_profiles =
      std::count_if(profile_attributes.begin(), profile_attributes.end(),
                    [](ProfileAttributesEntry* entry) {
                      return (base::Time::Now() - entry->GetActiveTime() <
                              kActiveTimeThreshold);
                    });
  // Don't show the profile picker at launch if the user has less than two
  // active profiles. However, if the user has already seen the profile picker
  // before, respect user's preference.
  if (number_of_active_profiles < 2 && !Shown())
    return false;

  bool pref_enabled = g_browser_process->local_state()->GetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup);
  base::UmaHistogramBoolean("ProfilePicker.AskOnStartup", pref_enabled);
  return pref_enabled;
}
