// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer_util.h"

#include <string_view>

#include "base/i18n/time_formatting.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/frame_node_impl_describer.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/page_node_impl_describer.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/process_node_impl_describer.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl_describer.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_type.h"

namespace performance_manager {

base::Value TimeDeltaToValue(base::TimeDelta delta) {
  std::u16string out;
  bool succeeded =
      TimeDurationFormatWithSeconds(delta, base::DURATION_WIDTH_SHORT, &out);
  DCHECK(succeeded);
  return base::Value(out);
}

base::Value TimeDeltaFromNowToValue(base::TimeTicks time_ticks) {
  return TimeDeltaToValue(base::TimeTicks::Now() - time_ticks);
}

base::Value TimeSinceEpochToValue(base::TimeTicks time_ticks) {
  const base::TimeDelta delta_since_epoch =
      time_ticks - base::TimeTicks::UnixEpoch();
  return base::Value(base::UnlocalizedTimeFormatWithPattern(
      base::Time::UnixEpoch() + delta_since_epoch, "yyyy-MM-dd HH:mm:ss"));
}

base::Value MaybeNullStringToValue(std::string_view str) {
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
  switch (node->GetNodeType()) {
    case NodeTypeEnum::kFrame:
      return FrameNodeImplDescriber()
          .DescribeNodeData(FrameNodeImpl::FromNode(node))
          .DebugString();
    case NodeTypeEnum::kPage:
      return PageNodeImplDescriber()
          .DescribeNodeData(PageNodeImpl::FromNode(node))
          .DebugString();
    case NodeTypeEnum::kProcess:
      return ProcessNodeImplDescriber()
          .DescribeNodeData(ProcessNodeImpl::FromNode(node))
          .DebugString();
    case NodeTypeEnum::kSystem:
      // SystemNodeImpl has no default describer. Return an empty dictionary.
      return base::Value::Dict().DebugString();
    case NodeTypeEnum::kWorker:
      return WorkerNodeImplDescriber()
          .DescribeNodeData(WorkerNodeImpl::FromNode(node))
          .DebugString();
  }
  NOTREACHED();
}

std::string DumpRegisteredDescribers(const Node* node) {
  return node->GetGraph()
      ->GetNodeDataDescriberRegistry()
      ->DescribeNodeData(node)
      .DebugString();
}

}  // namespace performance_manager
