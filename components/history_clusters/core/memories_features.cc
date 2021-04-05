// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_features.h"

#include "base/metrics/field_trial_params.h"

namespace memories {

GURL RemoteModelEndpoint() {
  return GURL(base::GetFieldTrialParamValueByFeature(
      kMemories, kRemoteModelEndpointParam));
}

// Enables the Chrome Memories history clustering feature.
const base::Feature kMemories{"Memories", base::FEATURE_DISABLED_BY_DEFAULT};
const char kRemoteModelEndpointParam[] = "MemoriesRemoteModelEndpoint";

// Enables debug info; e.g. shows visit metadata on chrome://history entries.
const base::Feature kDebug{"MemoriesDebug", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace memories
