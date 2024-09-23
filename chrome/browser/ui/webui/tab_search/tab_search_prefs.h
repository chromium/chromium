// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PREFS_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PREFS_H_

#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"

namespace user_prefs {

class PrefRegistrySyncable;

}  // namespace user_prefs

namespace tab_search_prefs {

extern const char kTabSearchRecentlyClosedSectionExpanded[];

extern const char kTabSearchTabIndex[];

extern const char kTabOrganizationFeature[];

extern const char kTabOrganizationShowFRE[];

extern const char kTabOrganizationModelStrategy[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

tab_search::mojom::TabOrganizationFeature GetTabOrganizationFeatureFromInt(
    const int feature);

int GetIntFromTabOrganizationFeature(
    const tab_search::mojom::TabOrganizationFeature feature);

}  // namespace tab_search_prefs

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_PREFS_H_
