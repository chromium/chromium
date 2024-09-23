// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/browser/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace tpcd::metadata {
namespace prefs {
const char kCohorts[] = "tpcd.metadata.cohorts";
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCohorts);
}
}  // namespace tpcd::metadata
