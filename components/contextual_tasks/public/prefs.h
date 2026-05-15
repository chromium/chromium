// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_PREFS_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_PREFS_H_

#include "base/values.h"

class PrefRegistrySimple;
class PrefService;

namespace contextual_tasks {

enum class SmartTabSharingSettingsValue {
  kEnabled = 0,
  kDisabled = 1,
};

extern const char kContextualTasksOnboardingTooltipDismissedCount[];
extern const char kContextualTasksShareOpenTabsEveryThread[];
extern const char kContextualTasksSiteExclusions[];
extern const char kContextualTasksSmartTabSharingSettings[];

// Registers profile prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// The `site_exclusions` dictionary uses lowercase domain names as keys,
// mapping to timestamp values. The timestamps are C++ double floating point
// values, the JavaScript-compatible number of milliseconds since Unix epoch.
void SaveSiteExclusionsToPrefs(PrefService* pref_service,
                               const base::DictValue& site_exclusions);

// Returns site exclusions dictionary with format described above.
const base::DictValue& ReadSiteExclusionsFromPrefs(PrefService* pref_service);

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_PREFS_H_
