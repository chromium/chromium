// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_

#include "base/feature_list.h"
#include "url/gurl.h"

namespace memories {

GURL RemoteModelEndpoint();

extern const base::Feature kMemories;
// The remote model endpoint used to cluster visits into memories.
extern const char kRemoteModelEndpointParam[];

// Enables debug features; e.g. displaying typed_count on chrome://history.
extern const base::Feature kDebug;

}  // namespace memories

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_FEATURES_H_
