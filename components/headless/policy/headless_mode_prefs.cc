// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/policy/headless_mode_prefs.h"

#include "components/headless/policy/headless_mode_policy.h"
#include "components/prefs/pref_registry_simple.h"

namespace headless {

namespace prefs {
// Defines administrator-set availability of the headless mode.
const char kHeadlessMode[] = "headless.mode";
}  // namespace prefs

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      headless::prefs::kHeadlessMode,
      static_cast<int>(HeadlessModePolicy::HeadlessMode::kDefaultValue));
}

}  // namespace headless
