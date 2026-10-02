// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prefs.h"

#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide::prefs {

namespace localstate {

// A pref that stores the last time a prediction model fetch was attempted. This
// limits the frequency of fetching for updates and prevents a crash loop that
// continually fetches prediction models on startup.
const char kModelLastFetchAttempt[] =
    "optimization_guide.predictionmodelfetcher.last_fetch_attempt";

// A pref that stores the last time a prediction model fetch was successful.
// This helps determine when to schedule the next fetch.
const char kModelLastFetchSuccess[] =
    "optimization_guide.predictionmodelfetcher.last_fetch_success";

// A dictionary pref that stores the lightweight metadata of all the models in
// the store, keyed by the optimization target and ModelCacheKey.
const char kModelStoreMetadata[] = "optimization_guide.model_store_metadata";

// A dictionary pref that stores the mapping between client generated
// ModelCacheKey based on the user profile characteristics and the server
// returned ModelCacheKey that was used in the actual model selection logic.
const char kModelCacheKeyMapping[] =
    "optimization_guide.model_cache_key_mapping";

// A dictionary pref that stores the file paths that need to be deleted as keys.
// The value will not be used.
const char kStoreFilePathsToDelete[] =
    "optimization_guide.store_file_paths_to_delete";

}  // namespace localstate

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(localstate::kModelLastFetchAttempt,
                             base::Time::Min(), PrefRegistry::LOSSY_PREF);
  registry->RegisterTimePref(localstate::kModelLastFetchSuccess,
                             base::Time::Min(), PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(localstate::kModelStoreMetadata);
  registry->RegisterDictionaryPref(localstate::kModelCacheKeyMapping);
  registry->RegisterDictionaryPref(localstate::kStoreFilePathsToDelete);
}

}  // namespace optimization_guide::prefs
