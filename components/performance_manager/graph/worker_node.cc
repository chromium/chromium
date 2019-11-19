// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/worker_node.h"

#include "base/logging.h"
#include "components/performance_manager/graph/worker_node_impl.h"

namespace performance_manager {

WorkerNode::WorkerNode() = default;
WorkerNode::~WorkerNode() = default;

WorkerNodeObserver::WorkerNodeObserver() = default;
WorkerNodeObserver::~WorkerNodeObserver() = default;

WorkerNode::ObserverDefaultImpl::ObserverDefaultImpl() = default;
WorkerNode::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

}  // namespace performance_manager
