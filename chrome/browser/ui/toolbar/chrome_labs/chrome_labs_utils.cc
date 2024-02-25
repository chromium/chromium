// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/channel_info.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/variations_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

bool IsFeatureSupportedOnChannel(const LabInfo& lab) {
  return chrome::GetChannel() <= lab.allowed_channel;
}

bool IsFeatureSupportedOnPlatform(const flags_ui::FeatureEntry* entry) {
  return entry && (entry->supported_platforms &
                   flags_ui::FlagsState::GetCurrentPlatform()) != 0;
}

bool IsChromeLabsFeatureValid(const LabInfo& lab, Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    // Tab Scrolling was added before new badge logic and is not a new
    // experiment. Adding it to |new_badge_prefs| will falsely indicate a new
    // experiment for the button’s dot indicator.
    if (IsChromeLabsFeatureValid(lab, profile) &&
        (lab.internal_name != flag_descriptions::kScrollableTabStripFlagId)) {
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode) ||
      !ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }
#endif

  return base::ranges::any_of(model->GetLabInfo(),
                              [&profile](const LabInfo& lab) {
                                return IsChromeLabsFeatureValid(lab, profile);
                              });
}

bool AreNewChromeLabsExperimentsAvailable(const ChromeLabsModel* model,
                                          Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ScopedDictPrefUpdate update(
      profile->GetPrefs(), chrome_labs_prefs::kChromeLabsNewBadgeDictAshChrome);
#else
  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              chrome_labs_prefs::kChromeLabsNewBadgeDict);
#endif

  base::Value::Dict& new_badge_prefs = update.Get();

  std::vector<std::string> lab_internal_names;
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();

  return base::ranges::any_of(
      all_labs.begin(), all_labs.end(), [&new_badge_prefs](const LabInfo& lab) {
        std::optional<int> new_badge_pref_value =
            new_badge_prefs.FindInt(lab.internal_name);
        // Show the dot indicator if new experiments have not been seen yet.
        return new_badge_pref_value ==
               chrome_labs_prefs::kChromeLabsNewExperimentPrefValue;
      });
}

bool IsChromeLabsEnabled() {
  // Always early out on the stable channel or if manually disabled regardless
  // of other conditions. The feature is enabled by default so if IsEnabled
  // returns false the feature will have been disabled.
  if (chrome::GetChannel() == version_info::Channel::STABLE ||
      !base::FeatureList::IsEnabled(features::kChromeLabs)) {
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
  if (g_browser_process->local_state()->GetInteger(
          chrome_labs_prefs::kChromeLabsActivationThreshold) <=
      features::kChromeLabsActivationPercentage.Get()) {
    return true;
  }
  return false;
}
