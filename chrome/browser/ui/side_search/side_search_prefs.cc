// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace side_search_prefs {

// Boolean pref indicating whether side search is enabled. This pref is mapped
// to an enterprise policy value.
const char kSideSearchEnabled[] = "side_search.enabled";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kSideSearchEnabled, true);
}

}  // namespace side_search_prefs
