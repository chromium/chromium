// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/origin_trial_prefs.h"

#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace embedder_support {

// static
void OriginTrialPrefs::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(embedder_support::prefs::kOriginTrialPublicKey,
                               "");
  registry->RegisterListPref(
      embedder_support::prefs::kOriginTrialDisabledFeatures);
  registry->RegisterListPref(
      embedder_support::prefs::kOriginTrialDisabledTokens);
}

}  // namespace embedder_support
