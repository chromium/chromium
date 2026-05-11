// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace personal_context::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kShouldShowPersonalContextFirstRunInfo, true);
}

}  // namespace personal_context::prefs
