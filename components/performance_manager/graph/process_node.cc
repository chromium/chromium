// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/process_node.h"

#include "base/logging.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

ProcessNode::ProcessNode() = default;
ProcessNode::~ProcessNode() = default;

ProcessNodeObserver::ProcessNodeObserver() = default;
ProcessNodeObserver::~ProcessNodeObserver() = default;

ProcessNode::ObserverDefaultImpl::ObserverDefaultImpl() = default;
ProcessNode::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

}  // namespace performance_manager
