// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/prefs.h"

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace contextual_tasks {

const char kContextualTasksOnboardingTooltipDismissedCount[] =
    "contextual_tasks.onboarding_tooltip_dismissed_count";

const char kContextualTasksShareOpenTabsEveryThread[] =
    "contextual_tasks.share_open_tabs_every_thread";

const char kContextualTasksSiteExclusions[] =
    "contextual_tasks.site_exclusions";

const char kContextualTasksSmartTabSharingSettings[] =
    "contextual_tasks.smart_tab_sharing_settings";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kContextualTasksSmartTabSharingSettings,
      static_cast<int>(SmartTabSharingSettingsValue::kEnabled));
}

void SaveSiteExclusionsToPrefs(PrefService* pref_service,
                               const base::DictValue& site_exclusions) {
  pref_service->SetDict(kContextualTasksSiteExclusions,
                        site_exclusions.Clone());
}

const base::DictValue& ReadSiteExclusionsFromPrefs(PrefService* pref_service) {
  return pref_service->GetDict(kContextualTasksSiteExclusions);
}

}  // namespace contextual_tasks
