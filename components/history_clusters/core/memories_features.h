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
// Note, the on-device backend (enabled by default) takes precedence over the
// remote model endpoint. The on-device backend has to be separately disabled
// to access the remote model endpoint.
GURL RemoteModelEndpoint();

// Returns the experiment name to pass through to the remote model debug
// endpoint to control how the visits get clustered. Returns an empty string if
// this client should just use be returned the default clustering or if the
// remote model debug endpoint is disabled.
extern const base::FeatureParam<std::string> kRemoteModelEndpointExperimentName;

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

// Enables the on-device clustering backend. Enabled by default, since this is
// the production mode of the whole feature. The backend is only in official
// builds, so it won't work in unofficial builds.
extern const base::FeatureParam<bool> kUseOnDeviceClusteringBackend;

// Features

// Enables the Chrome Memories history clustering feature.
extern const base::Feature kMemories;

// Enables debug info; e.g. shows visit metadata on chrome://history entries.
extern const base::Feature kDebug;

// Enables using a remote model endpoint for Memories clustering for debugging
// purposes. This should not be ever enabled in production.
extern const base::Feature kRemoteModelForDebugging;

// Enables persisting context annotations in the History DB. They are always
// calculated anyways. This just enables storing them. This is expected to be
// enabled for all users shortly. This just provides a killswitch.

// This flag is to enable us to turn on persisting context annotations WITHOUT
// exposing the Memories UI in general. If EITHER this flag or `kMemories` is
// enabled, users will have context annotations persisted into their History DB.
extern const base::Feature kPersistContextAnnotationsInHistoryDb;

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_
