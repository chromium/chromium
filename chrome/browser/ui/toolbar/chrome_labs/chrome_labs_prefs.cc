// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace chrome_labs_prefs {

// Boolean pref indicating whether Chrome Labs experimental features are enabled
// with toolbar entry point. This pref is mapped to an enterprise policy value
// that can be used to forcibly turn off Chrome Labs. Other conditions will
// still need to be met for the feature to be shown.
const char kBrowserLabsEnabledEnterprisePolicy[] = "browser_labs_enabled";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// For ash-chrome which will use profile prefs. Dictionary pref mapping
// experiment internal names to the day the experiment was added to the
// ChromeLabs bubble. This will be used to track time since the new badge was
// first shown.
const char kChromeLabsNewBadgeDictAshChrome[] =
    "chrome_labs_new_badge_dict_ash_chrome";

#else
// For all other desktop platforms which will use Local State. Dictionary pref
// mapping experiment internal names to the day the experiment was added to the
// ChromeLabs bubble. This will be used to track time since the new badge was
// first shown.
const char kChromeLabsNewBadgeDict[] = "chrome_labs_new_badge_dict";

#endif

// Integer pref used to determine whether Chrome Labs can be enabled.
// This is used to replace showing Chrome Labs through Finch.
// Values range from 1 - 100 inclusive. Chrome Labs is enabled on 99% pre-stable
// channels so values <= 99 correspond to having Chrome Labs enabled and a value
// of 100 corresponds to not having Chrome Labs enabled.
const char kChromeLabsActivationThreshold[] =
    "chrome_labs_activation_threshold";

// Default value if kChromeLabsActivationThreshold preference has not been
// set.
const int kChromeLabsActivationThresholdDefaultValue = -1;

// Sentinel time value for |kChromeLabsNewBadgeDict| pref to indicate the
// experiment is new but not seen by the user yet. Non-sentinel time values
// are guaranteed to be non-negative.
const int kChromeLabsNewExperimentPrefValue = -1;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kBrowserLabsEnabledEnterprisePolicy, true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterDictionaryPref(kChromeLabsNewBadgeDictAshChrome);
#endif
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kChromeLabsActivationThreshold,
                                kChromeLabsActivationThresholdDefaultValue);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterDictionaryPref(kChromeLabsNewBadgeDict);
#endif
}

}  // namespace chrome_labs_prefs
