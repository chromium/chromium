// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/frame_node.h"

#include "components/performance_manager/graph/frame_node_impl.h"

namespace performance_manager {

// static
const char* FrameNode::kDefaultPriorityReason =
    FrameNodeImpl::kDefaultPriorityReason;

FrameNode::FrameNode() = default;
FrameNode::~FrameNode() = default;

FrameNodeObserver::FrameNodeObserver() = default;
FrameNodeObserver::~FrameNodeObserver() = default;

FrameNode::ObserverDefaultImpl::ObserverDefaultImpl() = default;
FrameNode::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

}  // namespace performance_manager
