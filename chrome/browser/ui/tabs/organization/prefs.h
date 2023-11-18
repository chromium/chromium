// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_PREFS_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_PREFS_H_

#include "components/pref_registry/pref_registry_syncable.h"

namespace tab_organization_prefs {
extern const char kTabOrganizationNudgeBackoffCount[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* user_prefs);
}  // namespace tab_organization_prefs

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_PREFS_H_
