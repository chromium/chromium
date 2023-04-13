// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer_util.h"

#include "base/i18n/time_formatting.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/frame_node_impl_describer.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/node_type.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/page_node_impl_describer.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/process_node_impl_describer.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl_describer.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

base::Value TimeDeltaFromNowToValue(base::TimeTicks time_ticks) {
  base::TimeDelta delta = base::TimeTicks::Now() - time_ticks;

  std::u16string out;
  bool succeeded = TimeDurationFormat(delta, base::DURATION_WIDTH_WIDE, &out);
  DCHECK(succeeded);

  return base::Value(out);
}

base::Value MaybeNullStringToValue(base::StringPiece str) {
  if (str.data() == nullptr) {
    return base::Value();
  }
  return base::Value(str);
}

base::Value PriorityAndReasonToValue(
    const execution_context_priority::PriorityAndReason& priority_and_reason) {
  base::Value::Dict priority;
  priority.Set("priority",
               base::TaskPriorityToString(priority_and_reason.priority()));
  priority.Set("reason", MaybeNullStringToValue(priority_and_reason.reason()));
  return base::Value(std::move(priority));
}

std::string DumpNodeDescription(const Node* node) {
  const NodeBase* node_base = NodeBase::FromNode(node);
  switch (node_base->type()) {
    case NodeTypeEnum::kFrame:
      return FrameNodeImplDescriber()
          .DescribeNodeData(FrameNodeImpl::FromNodeBase(node_base))
          .DebugString();
    case NodeTypeEnum::kPage:
      return PageNodeImplDescriber()
          .DescribeNodeData(PageNodeImpl::FromNodeBase(node_base))
          .DebugString();
    case NodeTypeEnum::kProcess:
      return ProcessNodeImplDescriber()
          .DescribeNodeData(ProcessNodeImpl::FromNodeBase(node_base))
          .DebugString();
    case NodeTypeEnum::kSystem:
      // SystemNodeImpl has no default describer. Return an empty dictionary.
      return base::Value::Dict().DebugString();
    case NodeTypeEnum::kWorker:
      return WorkerNodeImplDescriber()
          .DescribeNodeData(WorkerNodeImpl::FromNodeBase(node_base))
          .DebugString();
    case NodeTypeEnum::kInvalidType:
      NOTREACHED_NORETURN();
  }
  NOTREACHED_NORETURN();
}

std::string DumpRegisteredDescribers(const Node* node) {
  return node->GetGraph()
      ->GetNodeDataDescriberRegistry()
      ->DescribeNodeData(node)
      .DebugString();
}

}  // namespace performance_manager
