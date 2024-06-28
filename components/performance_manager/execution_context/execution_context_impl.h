// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/public/execution_context/execution_context.h"

namespace performance_manager {

class FrameNode;
class WorkerNode;

namespace execution_context {

// An ExecutionContext implementation that wraps a FrameNodeImpl.
class FrameExecutionContext : public ExecutionContext,
                              public NodeInlineData<FrameExecutionContext> {
 public:
  explicit FrameExecutionContext(const FrameNodeImpl* frame_node);

  FrameExecutionContext(const FrameExecutionContext&) = delete;
  FrameExecutionContext& operator=(const FrameExecutionContext&) = delete;

  FrameExecutionContext(FrameExecutionContext&&);
  FrameExecutionContext& operator=(FrameExecutionContext&&);

  ~FrameExecutionContext() override;

  // ExecutionContext:
  ExecutionContextType GetType() const override;
  blink::ExecutionContextToken GetToken() const override;
  Graph* GetGraph() const override;
  const GURL& GetUrl() const override;
  const ProcessNode* GetProcessNode() const override;
  const PriorityAndReason& GetPriorityAndReason() const override;
  const FrameNode* GetFrameNode() const override;
  const WorkerNode* GetWorkerNode() const override;

 private:
  raw_ptr<const FrameNodeImpl> frame_node_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

// An ExecutionContext implementation that wraps a WorkerNodeImpl.
class WorkerExecutionContext : public ExecutionContext,
                               public NodeInlineData<WorkerExecutionContext> {
 public:
  explicit WorkerExecutionContext(const WorkerNodeImpl* worker_node);

  WorkerExecutionContext(const WorkerExecutionContext&) = delete;
  WorkerExecutionContext& operator=(const WorkerExecutionContext&) = delete;

  WorkerExecutionContext(WorkerExecutionContext&&);
  WorkerExecutionContext& operator=(WorkerExecutionContext&&);

  ~WorkerExecutionContext() override;

  // ExecutionContext:
  ExecutionContextType GetType() const override;
  blink::ExecutionContextToken GetToken() const override;
  Graph* GetGraph() const override;
  const GURL& GetUrl() const override;
  const ProcessNode* GetProcessNode() const override;
  const PriorityAndReason& GetPriorityAndReason() const override;
  const FrameNode* GetFrameNode() const override;
  const WorkerNode* GetWorkerNode() const override;

 private:
  raw_ptr<const WorkerNodeImpl> worker_node_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace execution_context
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_IMPL_H_
