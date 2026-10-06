// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_PREFS_H_

class PrefRegistrySimple;

namespace optimization_guide {

namespace prefs::localstate {

extern const char kModelQualityLoggingClientId[];

}  // namespace prefs::localstate

// Registers the model quality local state prefs.
void RegisterModelQualityLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_PREFS_H_
