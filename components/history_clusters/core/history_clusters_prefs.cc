// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace history_clusters {

namespace prefs {

// Whether History Clusters are visible to the user. True by default.
const char kVisible[] = "history_clusters.visible";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kVisible, true);
}

}  // namespace prefs

}  // namespace history_clusters
