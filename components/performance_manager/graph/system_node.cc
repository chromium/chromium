// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/system_node.h"

#include "base/logging.h"
#include "components/performance_manager/graph/system_node_impl.h"

namespace performance_manager {

SystemNode::SystemNode() = default;
SystemNode::~SystemNode() = default;

SystemNodeObserver::SystemNodeObserver() = default;
SystemNodeObserver::~SystemNodeObserver() = default;

SystemNode::ObserverDefaultImpl::ObserverDefaultImpl() = default;
SystemNode::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

}  // namespace performance_manager
