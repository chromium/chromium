// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/pref_names.h"

#include "base/files/file_path.h"
#include "components/prefs/pref_registry_simple.h"

namespace prefs {

const char kScreenAIScheduledDeletionTimePrefName[] =
    "accessibility.screen_ai.scheduled_deletion_time";

}  // namespace prefs

namespace screen_ai {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kScreenAIScheduledDeletionTimePrefName,
                             base::Time());
}

}  // namespace screen_ai