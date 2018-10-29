// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux_helper.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/welcome/nux/constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace nux {
// This feature flag is used to force the feature to be turned on for non-win
// and non-branded builds, like with tests or development on other platforms.
extern const base::Feature kNuxOnboardingForceEnabled{
    "NuxOnboardingForceEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNuxOnboardingEnabled(Profile* profile) {
  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    return true;
  } else {
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
    // To avoid diluting data collection, existing users should not be assigned
    // an NUX group. So, the kOnboardDuringNUX flag is used to short-circuit the
    // feature checks below.
    PrefService* prefs = profile->GetPrefs();
    bool onboard_during_nux =
        prefs && prefs->GetBoolean(prefs::kOnboardDuringNUX);

    return onboard_during_nux &&
           base::FeatureList::IsEnabled(nux::kNuxOnboardingFeature);
#else
    return false;
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  }
}
}  // namespace nux
