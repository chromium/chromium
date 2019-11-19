// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {

Graph::Graph() = default;
Graph::~Graph() = default;

GraphObserver::GraphObserver() = default;
GraphObserver::~GraphObserver() = default;

GraphOwned::GraphOwned() = default;
GraphOwned::~GraphOwned() = default;

GraphOwnedDefaultImpl::GraphOwnedDefaultImpl() = default;
GraphOwnedDefaultImpl::~GraphOwnedDefaultImpl() = default;

}  // namespace performance_manager
