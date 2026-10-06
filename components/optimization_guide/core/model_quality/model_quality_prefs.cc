// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide {

namespace prefs::localstate {

// An integer pref that contains the user's client id.
const char kModelQualityLoggingClientId[] =
    "optimization_guide.model_quality_logging_client_id";

}  // namespace prefs::localstate

void RegisterModelQualityLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::localstate::kModelQualityLoggingClientId,
                              0, PrefRegistry::LOSSY_PREF);
}

}  // namespace optimization_guide
