// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/variations_switches.h"
#include "components/webui/flags/feature_entry.h"
#include "components/webui/flags/pref_service_flags_storage.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace {
bool force_activation_for_testing = false;
}

bool IsFeatureSupportedOnChannel(const LabInfo& lab) {
  return chrome::GetChannel() <= lab.allowed_channel;
}

bool IsFeatureSupportedOnPlatform(const flags_ui::FeatureEntry* entry) {
  return entry && (entry->supported_platforms &
                   flags_ui::FlagsState::GetCurrentPlatform()) != 0;
}

bool IsChromeLabsFeatureValid(const LabInfo& lab, Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  PrefService* prefs = profile->GetPrefs();
#else
  PrefService* prefs = g_browser_process->local_state();
#endif
  // Note: Both ChromeOS owner and non-owner use PrefServiceFlagsStorage under
  // the hood. OwnersFlagsStorage has additional functionalities for setting
  // flags but since we are just reading the storage assume non-owner case and
  // bypass asynchronous owner check.
  auto flags_storage =
      std::make_unique<flags_ui::PrefServiceFlagsStorage>(prefs);

  const flags_ui::FeatureEntry* entry =
      about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
          lab.internal_name);

  return IsFeatureSupportedOnChannel(lab) &&
         IsFeatureSupportedOnPlatform(entry) &&
         !about_flags::ShouldSkipConditionalFeatureEntry(flags_storage.get(),
                                                         *entry);
}

void UpdateChromeLabsNewBadgePrefs(Profile* profile,
                                   const ChromeLabsModel* model) {
#if BUILDFLAG(IS_CHROMEOS)
  ScopedDictPrefUpdate update(
      profile->GetPrefs(), chrome_labs_prefs::kChromeLabsNewBadgeDictAshChrome);
#else
  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              chrome_labs_prefs::kChromeLabsNewBadgeDict);
#endif

  base::Value::Dict& new_badge_prefs = update.Get();

  std::vector<std::string> lab_internal_names;
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();
  for (const auto& lab : all_labs) {
    if (IsChromeLabsFeatureValid(lab, profile)) {
      lab_internal_names.push_back(lab.internal_name);
      if (!new_badge_prefs.Find(lab.internal_name)) {
        new_badge_prefs.Set(
            lab.internal_name,
            chrome_labs_prefs::kChromeLabsNewExperimentPrefValue);
      }
    }
  }
  std::vector<std::string> entries_to_remove;
  for (auto pref : new_badge_prefs) {
    // The size of |lab_internal_names| is capped around 3-5 elements.
    if (!base::Contains(lab_internal_names, pref.first)) {
      entries_to_remove.push_back(pref.first);
    }
  }

  for (const std::string& key : entries_to_remove) {
    new_badge_prefs.Remove(key);
  }
}

bool ShouldShowChromeLabsUI(const ChromeLabsModel* model, Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode) ||
      !ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }
#endif

  return std::ranges::any_of(model->GetLabInfo(),
                             [&profile](const LabInfo& lab) {
                               return IsChromeLabsFeatureValid(lab, profile);
                             });
}

bool AreNewChromeLabsExperimentsAvailable(const ChromeLabsModel* model,
                                          Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  ScopedDictPrefUpdate update(
      profile->GetPrefs(), chrome_labs_prefs::kChromeLabsNewBadgeDictAshChrome);
#else
  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              chrome_labs_prefs::kChromeLabsNewBadgeDict);
#endif

  base::Value::Dict& new_badge_prefs = update.Get();

  std::vector<std::string> lab_internal_names;
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();

  return std::ranges::any_of(
      all_labs.begin(), all_labs.end(), [&new_badge_prefs](const LabInfo& lab) {
        std::optional<int> new_badge_pref_value =
            new_badge_prefs.FindInt(lab.internal_name);
        // Show the dot indicator if new experiments have not been seen yet.
        return new_badge_pref_value ==
               chrome_labs_prefs::kChromeLabsNewExperimentPrefValue;
      });
}

bool IsChromeLabsEnabled() {
  // Always early out on the stable channel regardless of other conditions.
  if (chrome::GetChannel() == version_info::Channel::STABLE) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarking)) {
    return true;
  }
  // Could be null in unit tests.
  if (!g_browser_process->local_state()) {
    return false;
  }
  if (g_browser_process->local_state()->GetInteger(
          chrome_labs_prefs::kChromeLabsActivationThreshold) ==
      chrome_labs_prefs::kChromeLabsActivationThresholdDefaultValue) {
    g_browser_process->local_state()->SetInteger(
        chrome_labs_prefs::kChromeLabsActivationThreshold,
        base::RandInt(1, 100));
  }

  // The percentage of users that should see the feature.
  const int kChromeLabsActivationPercentage = 99;

  if (force_activation_for_testing ||
      g_browser_process->local_state()->GetInteger(
          chrome_labs_prefs::kChromeLabsActivationThreshold) <=
          kChromeLabsActivationPercentage) {
    return true;
  }
  return false;
}

void ForceChromeLabsActivationForTesting() {
  force_activation_for_testing = true;
}
