// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/query_clusters_state.h"

namespace history_clusters {

QueryClustersState::QueryClustersState(
    base::WeakPtr<HistoryClustersService> service)
    : service_(service) {}

QueryClustersState::~QueryClustersState() = default;

}  // namespace history_clusters
