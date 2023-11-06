// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace tab_search_prefs {

// Boolean pref indicating whether the Tab Search recently closed section is in
// an expanded state.
const char kTabSearchRecentlyClosedSectionExpanded[] =
    "tab_search.recently_closed_expanded";

// Integer pref indicating which tab the Tab Search bubble should open to
// when shown.
const char kTabSearchTabIndex[] = "tab_search.tab_index";

// Boolean pref indicating whether the user should see the first run experience
// when interacting with the Tab Organization UI.
const char kTabOrganizationShowFRE[] = "tab_organization.show_fre";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kTabSearchRecentlyClosedSectionExpanded, true);
  registry->RegisterIntegerPref(kTabSearchTabIndex, 0);
  registry->RegisterBooleanPref(kTabOrganizationShowFRE, true);
}

}  // namespace tab_search_prefs
