// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_

#include "base/feature_list.h"
#include "url/gurl.h"

namespace history_clusters {

// Returns the remote model debug endpoint used to cluster visits into memories.
// Returns an empty GURL() when the remote model debug endpoint is disabled.
GURL RemoteModelEndpointForDebugging();

// If enabled, completed visits are persisted to the history DB and read back
// when clustering. If disabled, completed visits are kept in-memory and used
// these in-memory visits are used when clustering.
bool StoreVisitsInHistoryDb();

// The max number of visits to use for each clustering iteration. When using the
// remote model, this limits the number of visits sent.
int MaxVisitsToCluster();

extern const base::Feature kMemories;
extern const base::Feature kDebug;
extern const base::Feature kRemoteModelForDebugging;

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_
