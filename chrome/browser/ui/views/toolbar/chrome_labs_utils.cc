// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_utils.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/pref_service_flags_storage.h"

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
