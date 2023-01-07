// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_utils.h"
#include "base/containers/contains.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/common/channel_info.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/scoped_user_pref_update.h"

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
                                   const ChromeLabsBubbleViewModel* model) {
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

  for (const std::string& key : entries_to_remove)
    new_badge_prefs.Remove(key);
}
