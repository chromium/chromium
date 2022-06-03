// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

base::Value NodeDataDescriberDefaultImpl::DescribeFrameNodeData(
    const FrameNode* node) const {
  return base::Value();
}

base::Value NodeDataDescriberDefaultImpl::DescribePageNodeData(
    const PageNode* node) const {
  return base::Value();
}

base::Value NodeDataDescriberDefaultImpl::DescribeProcessNodeData(
    const ProcessNode* node) const {
  return base::Value();
}

base::Value NodeDataDescriberDefaultImpl::DescribeSystemNodeData(
    const SystemNode* node) const {
  return base::Value();
}

base::Value NodeDataDescriberDefaultImpl::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  return base::Value();
}

}  // namespace performance_manager
