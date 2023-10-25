// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace history_clusters {

namespace prefs {

// Whether History Clusters are visible to the user. True by default.
const char kVisible[] = "history_clusters.visible";

// Dictionary containing the short keyword cache and associated timestamp.
const char kShortCache[] = "history_clusters.short_cache";

// Dictionary containing the "all keywords" cache and associated timestamp.
const char kAllCache[] = "history_clusters.all_cache";

// Integer controlling which tab should be opened by default.
const char kLastSelectedTab[] = "history_clusters.last_selected_tab";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kVisible, true);
  registry->RegisterDictionaryPref(prefs::kAllCache);
  registry->RegisterDictionaryPref(prefs::kShortCache);
  registry->RegisterIntegerPref(prefs::kLastSelectedTab, TabbedPage::DATE);
}

}  // namespace prefs

}  // namespace history_clusters
