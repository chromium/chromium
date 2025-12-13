// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_picker.h"

#include <string>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"

namespace {

bool g_open_command_line_urls_in_next_profile_opened = false;

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
}

}  // namespace

const char ProfilePicker::kTaskManagerUrl[] =
    "chrome://profile-picker/task-manager";

ProfilePicker::Params::~Params() = default;

ProfilePicker::Params::Params(ProfilePicker::Params&&) = default;

ProfilePicker::Params& ProfilePicker::Params::operator=(
    ProfilePicker::Params&&) = default;

// static
ProfilePicker::Params ProfilePicker::Params::FromEntryPoint(
    EntryPoint entry_point) {
  // Use specialized constructors when available.
  CHECK_NE(entry_point, EntryPoint::kBackgroundModeManager);
  CHECK_NE(entry_point, EntryPoint::kGlicManager);
  return ProfilePicker::Params(entry_point, GetPickerProfilePath());
}

ProfilePicker::Params ProfilePicker::Params::FromStartupWithEmail(
    const std::string& email) {
  Params params = ProfilePicker::Params::FromEntryPoint(
      EntryPoint::kOnStartupCreateProfileWithEmail);
  params.initial_email_ = email;
  return params;
}

// static
ProfilePicker::Params ProfilePicker::Params::ForBackgroundManager(
    const GURL& on_select_profile_target_url) {
  Params params(EntryPoint::kBackgroundModeManager, GetPickerProfilePath());
  params.on_select_profile_target_url_ = on_select_profile_target_url;
  return params;
}

// static
ProfilePicker::Params ProfilePicker::Params::ForFirstRun(
    const base::FilePath& profile_path,
    FirstRunExitedCallback first_run_exited_callback) {
  Params params(EntryPoint::kFirstRun, profile_path);
  params.first_run_exited_callback_ = std::move(first_run_exited_callback);
  return params;
}

// static
ProfilePicker::Params ProfilePicker::Params::ForGlicManager(
    base::OnceCallback<void(Profile*)> picked_profile_callback) {
  Params params(EntryPoint::kGlicManager, GetPickerProfilePath());
  params.picked_profile_callback_ = std::move(picked_profile_callback);
  return params;
}

void ProfilePicker::Params::NotifyFirstRunExited(
    FirstRunExitStatus exit_status) {
  if (!first_run_exited_callback_) {
    return;
  }
  std::move(first_run_exited_callback_).Run(exit_status);
}

void ProfilePicker::Params::NotifyProfilePicked(Profile* profile) {
  CHECK(picked_profile_callback_);
  CHECK_EQ(entry_point_, EntryPoint::kGlicManager);
  std::move(picked_profile_callback_).Run(profile);
}

bool ProfilePicker::Params::CanReusePickerWindow(const Params& other) const {
  LOG(WARNING) << "Checking window reusability from entry point "
               << static_cast<int>(entry_point_) << " to "
               << static_cast<int>(other.entry_point());

  // Some entry points have specific UIs that cannot be reused for other entry
  // points.
  base::flat_set<EntryPoint> exclusive_entry_points = {
      EntryPoint::kFirstRun,
      EntryPoint::kGlicManager,
  };
  if (entry_point_ != other.entry_point_ &&
      (exclusive_entry_points.contains(entry_point_) ||
       exclusive_entry_points.contains(other.entry_point_))) {
    return false;
  }
  return profile_path_ == other.profile_path_;
}

ProfilePicker::Params::Params(EntryPoint entry_point,
                              const base::FilePath& profile_path)
    : entry_point_(entry_point), profile_path_(profile_path) {}

// static
bool ProfilePicker::Shown() {
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  return prefs->GetBoolean(prefs::kBrowserProfilePickerShown);
}

// static
StartupProfileMode ProfilePicker::GetStartupMode() {
  AvailabilityOnStartup availability_on_startup = GetAvailabilityOnStartup();

  if (availability_on_startup == AvailabilityOnStartup::kDisabled) {
    return StartupProfileMode::kBrowserWindow;
  }

  // TODO (crbug/1155158): Move this over the urls check (in
  // startup_browser_creator.cc) once the profile picker can forward urls
  // specified in command line.
  if (availability_on_startup == AvailabilityOnStartup::kForced) {
    return StartupProfileMode::kProfilePicker;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Only launch the profile creation flow at startup if the user has specified
  // both a profile email address and the switch to create a new profile. Only
  // launch the profile creation flow if and a profile with this email does not
  // already exist.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kProfileEmail)) {
    std::string switch_email =
        command_line->GetSwitchValueASCII(switches::kProfileEmail);
    if (!switch_email.empty()) {
      if (!profile_manager->GetProfileDirForEmail(switch_email).empty()) {
        return StartupProfileMode::kBrowserWindow;
      } else if (command_line->HasSwitch(
                     switches::kCreateProfileEmailIfNotExists) &&
                 base::FeatureList::IsEnabled(
                     features::kCreateProfileIfNoneExists)) {
        return StartupProfileMode::kProfilePicker;
      }
    }
  }

  size_t number_of_profiles = profile_manager->GetNumberOfProfiles();
  // Need to consider 0 profiles as this is what happens in some browser-tests.
  if (number_of_profiles == 0) {
    return StartupProfileMode::kBrowserWindow;
  }
  if (number_of_profiles == 1 &&
      !base::FeatureList::IsEnabled(
          switches::kShowProfilePickerToAllUsersExperiment)) {
    return StartupProfileMode::kBrowserWindow;
  }

  bool pref_enabled = g_browser_process->local_state()->GetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup);
  base::UmaHistogramBoolean("ProfilePicker.AskOnStartup", pref_enabled);
  if (pref_enabled) {
    return StartupProfileMode::kProfilePicker;
  }
  return StartupProfileMode::kBrowserWindow;
}

// static
void ProfilePicker::SetOpenCommandLineUrlsInNextProfileOpened(bool value) {
  g_open_command_line_urls_in_next_profile_opened = value;
}

// static
bool ProfilePicker::GetOpenCommandLineUrlsInNextProfileOpened() {
  return g_open_command_line_urls_in_next_profile_opened;
}
