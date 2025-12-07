// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_types.h"

namespace history_clusters {

QueryClustersFilterParams::QueryClustersFilterParams() = default;

QueryClustersFilterParams::QueryClustersFilterParams(
    const QueryClustersFilterParams&) = default;

QueryClustersFilterParams::QueryClustersFilterParams(
    QueryClustersFilterParams&&) = default;

QueryClustersFilterParams& QueryClustersFilterParams::operator=(
    const QueryClustersFilterParams&) = default;

QueryClustersFilterParams& QueryClustersFilterParams::operator=(
    QueryClustersFilterParams&&) = default;

QueryClustersFilterParams::~QueryClustersFilterParams() = default;

IncompleteVisitContextAnnotations::IncompleteVisitContextAnnotations() =
    default;

IncompleteVisitContextAnnotations::IncompleteVisitContextAnnotations(
    const IncompleteVisitContextAnnotations&) = default;

IncompleteVisitContextAnnotations::~IncompleteVisitContextAnnotations() =
    default;

}  // namespace history_clusters
