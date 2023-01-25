// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl_describer.h"

#include "base/strings/string_number_conversions.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "WorkerNode";

const char* WorkerTypeToString(WorkerNode::WorkerType state) {
  switch (state) {
    case WorkerNode::WorkerType::kDedicated:
      return "kDedicated";
    case WorkerNode::WorkerType::kShared:
      return "kShared";
    case WorkerNode::WorkerType::kService:
      return "kService";
  }
}

}  // namespace

void WorkerNodeImplDescriber::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void WorkerNodeImplDescriber::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value WorkerNodeImplDescriber::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  const WorkerNodeImpl* impl = WorkerNodeImpl::FromNode(node);
  if (!impl)
    return base::Value();

  base::Value::Dict ret;
  ret.Set("browser_context_id", impl->browser_context_id());
  ret.Set("worker_token", impl->worker_token().ToString());
  ret.Set("url", impl->url().spec());
  ret.Set("worker_type", WorkerTypeToString(impl->worker_type()));
  ret.Set("priority", PriorityAndReasonToValue(impl->priority_and_reason()));

  return base::Value(std::move(ret));
}

}  // namespace performance_manager
