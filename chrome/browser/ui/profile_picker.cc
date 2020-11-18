// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_picker.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

// static
bool ProfilePicker::ShouldShowAtLaunch() {
  size_t number_of_profiles = g_browser_process->profile_manager()
                                  ->GetProfileAttributesStorage()
                                  .GetNumberOfProfiles();
  // Need to consider 0 profiles as this is what happens in some browser-tests.
  if (signin_util::IsForceSigninEnabled() || number_of_profiles <= 1)
    return false;

  bool pref_enabled = g_browser_process->local_state()->GetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup);
  base::UmaHistogramBoolean("ProfilePicker.AskOnStartup", pref_enabled);
  return pref_enabled &&
         base::FeatureList::IsEnabled(features::kNewProfilePicker);
}
