// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/safety_check_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/safety_check/safety_check_pref_names.h"

namespace safety_check::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kSafetyCheckHomeModuleEnabled, true);
}

}  // namespace safety_check::prefs
