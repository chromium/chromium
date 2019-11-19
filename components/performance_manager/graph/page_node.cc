// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/page_node.h"

#include "base/logging.h"
#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

PageNode::PageNode() = default;
PageNode::~PageNode() = default;

PageNodeObserver::PageNodeObserver() = default;
PageNodeObserver::~PageNodeObserver() = default;

PageNode::ObserverDefaultImpl::ObserverDefaultImpl() = default;
PageNode::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

}  // namespace performance_manager
