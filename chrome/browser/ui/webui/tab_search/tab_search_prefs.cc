// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"

#include <utility>

#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace tab_search_prefs {

// Boolean pref indicating whether the Tab Search recently closed section is in
// an expanded state.
const char kTabSearchRecentlyClosedSectionExpanded[] =
    "tab_search.recently_closed_expanded";

// Boolean pref indicating whether the Tab Search bubble has been used (a tab
// has been activated or closed).
const char kTabSearchUsed[] = "tab_search.used";

// Boolean pref indicating whether the user should see the first run experience
// when interacting with the Tab Organization UI.
const char kTabOrganizationShowFRE[] = "tab_organization.show_fre_2";

// Integer pref indicating which model strategy the user would like their tabs
// to be organized according to.
const char kTabOrganizationModelStrategy[] = "tab_organization.model_strategy";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kTabSearchRecentlyClosedSectionExpanded, true);
  registry->RegisterBooleanPref(kTabSearchUsed, false);
  registry->RegisterBooleanPref(kTabOrganizationShowFRE, true);
  registry->RegisterIntegerPref(kTabOrganizationModelStrategy, 0);
}

}  // namespace tab_search_prefs
