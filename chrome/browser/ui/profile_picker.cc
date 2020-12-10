// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_picker.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if defined(OS_WIN)
#include "chrome/browser/notifications/win/notification_launch_id.h"
#endif

namespace {

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

// static
bool ProfilePicker::ShouldShowAtLaunch(
    const base::CommandLine& command_line,
    const std::vector<GURL>& urls_to_launch) {
  AvailabilityOnStartup availability_on_startup = GetAvailabilityOnStartup();

  // Don't show the picker if a certain profile (or an incognito window in the
  // default profile) is explicitly requested.
  if (profiles::IsGuestModeRequested(command_line,
                                     g_browser_process->local_state(),
                                     /*show_warning=*/false) ||
      command_line.HasSwitch(switches::kIncognito) ||
      command_line.HasSwitch(switches::kProfileDirectory)) {
    return false;
  }

  // Don't show the picker if an app is explicitly requested to open. This URL
  // param should be ideally paired with switches::kProfileDirectory but it's
  // better to err on the side of opening the last profile than to err on the
  // side of not opening the app directly.
  if (command_line.HasSwitch(switches::kApp) ||
      command_line.HasSwitch(switches::kAppId)) {
    return false;
  }

// If the browser is launched due to activation on Windows native notification,
// the profile id encoded in the notification launch id should be chosen over
// the profile picker.
#if defined(OS_WIN)
  std::string profile_id =
      NotificationLaunchId::GetNotificationLaunchProfileId(command_line);
  if (!profile_id.empty()) {
    return false;
  }
#endif  // defined(OS_WIN)

  // Don't show the picker if a any URL is requested to launch via the
  // command-line.
  if (!urls_to_launch.empty()) {
    return false;
  }

  if (signin_util::IsForceSigninEnabled())
    return false;

  if (!base::FeatureList::IsEnabled(features::kNewProfilePicker))
    return false;

  // TODO (crbug/1155158): Move this over the urls check once the
  // profile picker can forward urls specified in command line.
  if (availability_on_startup == AvailabilityOnStartup::kForced)
    return true;

  size_t number_of_profiles = g_browser_process->profile_manager()
                                  ->GetProfileAttributesStorage()
                                  .GetNumberOfProfiles();

  if (number_of_profiles == 1)
    return false;

  bool pref_enabled = g_browser_process->local_state()->GetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup);
  base::UmaHistogramBoolean("ProfilePicker.AskOnStartup", pref_enabled);
  return pref_enabled;
}
