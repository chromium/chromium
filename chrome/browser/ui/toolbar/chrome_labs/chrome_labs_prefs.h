// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_PREFS_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_PREFS_H_

#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"

class PrefRegistrySimple;
namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chrome_labs_prefs {

extern const char kBrowserLabsEnabledEnterprisePolicy[];
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kChromeLabsNewBadgeDictAshChrome[];
#else
extern const char kChromeLabsNewBadgeDict[];
#endif

extern const char kChromeLabsActivationThreshold[];

extern const int kChromeLabsActivationThresholdDefaultValue;

extern const int kChromeLabsNewExperimentPrefValue;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace chrome_labs_prefs

#endif  // CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_PREFS_H_
