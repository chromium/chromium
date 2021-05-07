// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "url/gurl.h"

namespace history_clusters {

// Params & helpers functions

// Returns the remote model debug endpoint used to cluster visits into memories.
// Returns an empty GURL() when the remote model debug endpoint is disabled.
GURL RemoteModelEndpoint();

// Returns the experiment name to pass through to the remote model debug
// endpoint to control how the visits get clustered. Returns an empty string if
// this client should just use be returned the default clustering or if the
// remote model debug endpoint is disabled.
extern const base::FeatureParam<std::string> kRemoteModelEndpointExperimentName;

// If enabled, completed visits context annotations are persisted to the history
// DB and read back when clustering. If disabled, completed visit context
// annotations are kept in-memory and used these in-memory visits are used when
// clustering.
extern const base::FeatureParam<bool> kPersistContextAnnotationsInHistoryDb;

// The max number of visits to use for each clustering iteration. When using the
// remote model, this limits the number of visits sent.  Only applies when using
// persisted visit context annotations; i.e.
// `kPersistContextAnnotationsInHistoryDb` is true.
extern const base::FeatureParam<int> kMaxVisitsToCluster;

// The recency threshold controlling which visits will be clustered. This isn't
// the only factor; i.e. visits older than `MaxDaysToCluster()` may still be
// clustered. Only applies when using persisted visit context annotations; i.e.
// `kPersistContextAnnotationsInHistoryDb` is true.
extern const base::FeatureParam<int> kMaxDaysToCluster;

// If enabled, updating clusters will persist the results to the history DB and
// accessing clusters will retrieve them from the history DB. If disabled,
// updating clusters is a no-op and accessing clusters will generate and return
// new clusters without persisting them.
extern const base::FeatureParam<bool> kPersistClustersInHistoryDb;

// Features

// Enables the Chrome Memories history clustering feature.
extern const base::Feature kMemories;

// Enables debug info; e.g. shows visit metadata on chrome://history entries.
extern const base::Feature kDebug;

// Enables using a remote model endpoint for Memories clustering for debugging
// purposes. This should not be ever enabled in production.
extern const base::Feature kRemoteModelForDebugging;

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_
