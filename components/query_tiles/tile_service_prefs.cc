// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/tile_service_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace query_tiles {

constexpr char kBackoffEntryKey[] = "query_tiles.backoff_entry_key";
constexpr char kFirstScheduleTimeKey[] = "query_tiles.first_schedule_time_key";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kBackoffEntryKey);
  registry->RegisterTimePref(kFirstScheduleTimeKey, base::Time());
}
}  // namespace query_tiles
