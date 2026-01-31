// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

base::DictValue NodeDataDescriberDefaultImpl::DescribeFrameNodeData(
    const FrameNode* node) const {
  return base::DictValue();
}

base::DictValue NodeDataDescriberDefaultImpl::DescribePageNodeData(
    const PageNode* node) const {
  return base::DictValue();
}

base::DictValue NodeDataDescriberDefaultImpl::DescribeProcessNodeData(
    const ProcessNode* node) const {
  return base::DictValue();
}

base::DictValue NodeDataDescriberDefaultImpl::DescribeSystemNodeData(
    const SystemNode* node) const {
  return base::DictValue();
}

base::DictValue NodeDataDescriberDefaultImpl::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  return base::DictValue();
}

}  // namespace performance_manager
