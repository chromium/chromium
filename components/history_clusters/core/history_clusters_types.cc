// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_types.h"

namespace history_clusters {

Visit::Visit() = default;
Visit::~Visit() = default;
Visit::Visit(const Visit&) = default;
Visit::Visit(Visit&&) = default;
Visit& Visit::operator=(const Visit&) = default;
Visit& Visit::operator=(Visit&&) = default;

Cluster::Cluster() = default;
Cluster::~Cluster() = default;
Cluster::Cluster(const Cluster&) = default;
Cluster::Cluster(Cluster&&) = default;
Cluster& Cluster::operator=(const Cluster&) = default;
Cluster& Cluster::operator=(Cluster&&) = default;

QueryClustersResult::QueryClustersResult() = default;
QueryClustersResult::~QueryClustersResult() = default;
QueryClustersResult::QueryClustersResult(const QueryClustersResult&) = default;

}  // namespace history_clusters
