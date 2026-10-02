// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREFS_H_

class PrefRegistrySimple;

namespace optimization_guide::prefs {

namespace localstate {

extern const char kModelLastFetchAttempt[];
extern const char kModelLastFetchSuccess[];
extern const char kModelStoreMetadata[];
extern const char kModelCacheKeyMapping[];
extern const char kStoreFilePathsToDelete[];

}  // namespace localstate

// Registers the prediction delivery local state prefs.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace optimization_guide::prefs

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREFS_H_
