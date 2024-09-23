// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl_describer.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/performance_manager/graph/worker_node_impl.h"
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

base::Value::Dict WorkerNodeImplDescriber::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  const WorkerNodeImpl* impl = WorkerNodeImpl::FromNode(node);
  if (!impl)
    return base::Value::Dict();

  base::Value::Dict ret;
  ret.Set("worker_type", WorkerTypeToString(impl->GetWorkerType()));
  ret.Set("browser_context_id", impl->GetBrowserContextID());
  ret.Set("worker_token", impl->GetWorkerToken().ToString());
  ret.Set("resource_context", impl->GetResourceContext().ToString());
  ret.Set("url", impl->GetURL().spec());
  ret.Set("origin", impl->GetOrigin().GetDebugString());
  ret.Set("priority", PriorityAndReasonToValue(impl->GetPriorityAndReason()));

  base::Value::Dict metrics;
  metrics.Set("resident_set",
              base::NumberToString(impl->GetResidentSetKbEstimate()));
  metrics.Set("private_footprint",
              base::NumberToString(impl->GetPrivateFootprintKbEstimate()));
  ret.Set("metrics_estimates", std::move(metrics));

  return ret;
}

}  // namespace performance_manager
