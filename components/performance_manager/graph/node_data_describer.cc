// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

base::Value::Dict NodeDataDescriberDefaultImpl::DescribeFrameNodeData(
    const FrameNode* node) const {
  return base::Value::Dict();
}

base::Value::Dict NodeDataDescriberDefaultImpl::DescribePageNodeData(
    const PageNode* node) const {
  return base::Value::Dict();
}

base::Value::Dict NodeDataDescriberDefaultImpl::DescribeProcessNodeData(
    const ProcessNode* node) const {
  return base::Value::Dict();
}

base::Value::Dict NodeDataDescriberDefaultImpl::DescribeSystemNodeData(
    const SystemNode* node) const {
  return base::Value::Dict();
}

base::Value::Dict NodeDataDescriberDefaultImpl::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  return base::Value::Dict();
}

}  // namespace performance_manager
