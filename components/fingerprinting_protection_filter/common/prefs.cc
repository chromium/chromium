// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/common/prefs.h"

#include "components/fingerprinting_protection_filter/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace fingerprinting_protection_filter::prefs {
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kRefreshHeuristicBreakageException);
}
}  // namespace fingerprinting_protection_filter::prefs
