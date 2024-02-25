// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/prefs.h"

namespace tab_organization_prefs {

// Nonnegative integer pref counting the net number of times we've backed off on
// the proactive nudge for this user. Incremented each time the user dismisses
// the nudge, and decremented each time the user clicks through the nudge.
const char kTabOrganizationNudgeBackoffCount[] =
    "tab_organization.nudge_backoff_count";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(kTabOrganizationNudgeBackoffCount, 0);
}

}  // namespace tab_organization_prefs
