// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"

#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
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

// Integer pref indicating which organization feature, if any, the Tab
// Organization Selector should open to when shown.
const char kTabOrganizationFeature[] = "tab_organization.feature";

// Boolean pref indicating whether the user should see the first run experience
// when interacting with the Tab Organization UI.
const char kTabOrganizationShowFRE[] = "tab_organization.show_fre_2";

// Integer pref indicating which model strategy the user would like their tabs
// to be organized according to.
const char kTabOrganizationModelStrategy[] = "tab_organization.model_strategy";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kTabSearchRecentlyClosedSectionExpanded, true);
  registry->RegisterIntegerPref(kTabSearchTabIndex, 0);
  registry->RegisterIntegerPref(
      kTabOrganizationFeature,
      GetIntFromTabOrganizationFeature(
          tab_search::mojom::TabOrganizationFeature::kSelector));
  registry->RegisterBooleanPref(kTabOrganizationShowFRE, true);
  registry->RegisterIntegerPref(kTabOrganizationModelStrategy, 0);
}

tab_search::mojom::TabOrganizationFeature GetTabOrganizationFeatureFromInt(
    const int feature) {
  return ToKnownEnumValue(
      static_cast<tab_search::mojom::TabOrganizationFeature>(feature));
}

int GetIntFromTabOrganizationFeature(
    const tab_search::mojom::TabOrganizationFeature feature) {
  return base::to_underlying(feature);
}

}  // namespace tab_search_prefs
