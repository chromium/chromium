// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context/execution_context_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"

namespace performance_manager {
namespace execution_context {

// Allows accessing internal NodeAttachedData storage for ExecutionContext
// implementations.
class ExecutionContextAccess {
 public:
  using PassKey = base::PassKey<ExecutionContextAccess>;

  template <typename NodeImplType>
  static std::unique_ptr<NodeAttachedData>* GetExecutionAccessStorage(
      NodeImplType* node) {
    return node->GetExecutionContextStorage(PassKey());
  }
};

namespace {

// Templated partial implementation of ExecutionContext. Implements the common
// bits of ExecutionContext, except for GetToken().
template <typename DerivedType,
          typename NodeImplType,
          ExecutionContextType kExecutionContextType>
class ExecutionContextImpl : public ExecutionContext,
                             public NodeAttachedDataImpl<DerivedType> {
 public:
  using ImplType = ExecutionContextImpl;

  struct Traits : public NodeAttachedDataImpl<DerivedType>::
                      template NodeAttachedDataOwnedByNodeType<NodeImplType> {};

  ExecutionContextImpl(const ExecutionContextImpl&) = delete;
  ExecutionContextImpl& operator=(const ExecutionContextImpl&) = delete;
  ~ExecutionContextImpl() override {}

  // ExecutionContext implementation:
  ExecutionContextType GetType() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return kExecutionContextType;
  }

  Graph* GetGraph() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node_->graph();
  }

  const GURL& GetUrl() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node_->url();
  }

  const ProcessNode* GetProcessNode() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node_->process_node();
  }

  // Returns the current priority of the execution context, and the reason for
  // the execution context having that particular priority.
  const PriorityAndReason& GetPriorityAndReason() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node_->priority_and_reason();
  }

  const FrameNode* GetFrameNode() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return nullptr;
  }

  const WorkerNode* GetWorkerNode() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return nullptr;
  }

 protected:
  explicit ExecutionContextImpl(const NodeImplType* node) : node_(node) {
    DCHECK(node);
  }

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      NodeImplType* node) {
    return ExecutionContextAccess::GetExecutionAccessStorage(node);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<const NodeImplType> node_ = nullptr;
};

// An ExecutionContext implementation that wraps a FrameNodeImpl.
class FrameExecutionContext
    : public ExecutionContextImpl<FrameExecutionContext,
                                  FrameNodeImpl,
                                  ExecutionContextType::kFrameNode> {
 public:
  FrameExecutionContext(const FrameExecutionContext&) = delete;
  FrameExecutionContext& operator=(const FrameExecutionContext&) = delete;
  ~FrameExecutionContext() override = default;

  // Remaining ExecutionContext implementation not provided by
  // ExecutionContextImpl:
  blink::ExecutionContextToken GetToken() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return blink::ExecutionContextToken(node_->frame_token());
  }

  const FrameNode* GetFrameNode() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node_;
  }

 protected:
  friend class NodeAttachedDataImpl<FrameExecutionContext>;
  explicit FrameExecutionContext(const FrameNodeImpl* frame_node)
      : ImplType(frame_node) {}
};

// An ExecutionContext implementation that wraps a WorkerNodeImpl.
class WorkerExecutionContext
    : public ExecutionContextImpl<WorkerExecutionContext,
                                  WorkerNodeImpl,
                                  ExecutionContextType::kWorkerNode> {
 public:
  WorkerExecutionContext(const WorkerExecutionContext&) = delete;
  WorkerExecutionContext& operator=(const WorkerExecutionContext&) = delete;
  ~WorkerExecutionContext() override = default;

  // Remaining ExecutionContext implementation not provided by
  // ExecutionContextImpl:
  blink::ExecutionContextToken GetToken() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ToExecutionContextToken(node_->worker_token());
  }

  const WorkerNode* GetWorkerNode() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node_;
  }

 protected:
  friend class NodeAttachedDataImpl<WorkerExecutionContext>;
  explicit WorkerExecutionContext(const WorkerNodeImpl* worker_node)
      : ImplType(worker_node) {}
};

}  // namespace

// Declared in execution_context.h.
blink::ExecutionContextToken ToExecutionContextToken(
    const blink::WorkerToken& token) {
  if (token.Is<blink::DedicatedWorkerToken>()) {
    return blink::ExecutionContextToken(
        token.GetAs<blink::DedicatedWorkerToken>());
  }
  if (token.Is<blink::ServiceWorkerToken>()) {
    return blink::ExecutionContextToken(
        token.GetAs<blink::ServiceWorkerToken>());
  }
  if (token.Is<blink::SharedWorkerToken>()) {
    return blink::ExecutionContextToken(
        token.GetAs<blink::SharedWorkerToken>());
  }
  // Unfortunately there's no enum of input types, so no way to ensure that
  // all types are handled at compile time. This at least ensures via the CQ
  // that all types are handled.
  NOTREACHED();
  return blink::ExecutionContextToken();
}

// Declared in execution_context.h.
// static
const ExecutionContext* ExecutionContext::From(const FrameNode* frame_node) {
  return ExecutionContextRegistry::GetExecutionContextForFrameNode(frame_node);
}

// Declared in execution_context.h.
// static
const ExecutionContext* ExecutionContext::From(const WorkerNode* worker_node) {
  return ExecutionContextRegistry::GetExecutionContextForWorkerNode(
      worker_node);
}

const ExecutionContext* GetOrCreateExecutionContextForFrameNode(
    const FrameNode* frame_node) {
  DCHECK(frame_node);
  DCHECK_ON_GRAPH_SEQUENCE(frame_node->GetGraph());
  return FrameExecutionContext::GetOrCreate(
      FrameNodeImpl::FromNode(frame_node));
}

const ExecutionContext* GetOrCreateExecutionContextForWorkerNode(
    const WorkerNode* worker_node) {
  DCHECK(worker_node);
  DCHECK_ON_GRAPH_SEQUENCE(worker_node->GetGraph());
  return WorkerExecutionContext::GetOrCreate(
      WorkerNodeImpl::FromNode(worker_node));
}

}  // namespace execution_context
}  // namespace performance_manager
