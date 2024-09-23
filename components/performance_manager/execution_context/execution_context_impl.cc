// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context/execution_context_impl.h"

#include "base/sequence_checker.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
namespace execution_context {

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
  NOTREACHED_IN_MIGRATION();
  return blink::ExecutionContextToken();
}

FrameExecutionContext::FrameExecutionContext(const FrameNodeImpl* frame_node)
    : frame_node_(frame_node) {}

FrameExecutionContext::FrameExecutionContext(FrameExecutionContext&&) = default;

FrameExecutionContext& FrameExecutionContext::operator=(
    FrameExecutionContext&&) = default;

FrameExecutionContext::~FrameExecutionContext() = default;

ExecutionContextType FrameExecutionContext::GetType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ExecutionContextType::kFrameNode;
}

blink::ExecutionContextToken FrameExecutionContext::GetToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return blink::ExecutionContextToken(frame_node_->GetFrameToken());
}

Graph* FrameExecutionContext::GetGraph() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_node_->graph();
}

const GURL& FrameExecutionContext::GetUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_node_->GetURL();
}

const ProcessNode* FrameExecutionContext::GetProcessNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_node_->process_node();
}

const PriorityAndReason& FrameExecutionContext::GetPriorityAndReason() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_node_->GetPriorityAndReason();
}

const FrameNode* FrameExecutionContext::GetFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_node_;
}

const WorkerNode* FrameExecutionContext::GetWorkerNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

WorkerExecutionContext::WorkerExecutionContext(
    const WorkerNodeImpl* worker_node)
    : worker_node_(worker_node) {}

WorkerExecutionContext::WorkerExecutionContext(WorkerExecutionContext&&) =
    default;

WorkerExecutionContext& WorkerExecutionContext::operator=(
    WorkerExecutionContext&&) = default;

WorkerExecutionContext::~WorkerExecutionContext() = default;

ExecutionContextType WorkerExecutionContext::GetType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ExecutionContextType::kWorkerNode;
}

blink::ExecutionContextToken WorkerExecutionContext::GetToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ToExecutionContextToken(worker_node_->GetWorkerToken());
}

Graph* WorkerExecutionContext::GetGraph() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_node_->graph();
}

const GURL& WorkerExecutionContext::GetUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_node_->GetURL();
}

const ProcessNode* WorkerExecutionContext::GetProcessNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_node_->process_node();
}

const PriorityAndReason& WorkerExecutionContext::GetPriorityAndReason() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_node_->GetPriorityAndReason();
}

const FrameNode* WorkerExecutionContext::GetFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

const WorkerNode* WorkerExecutionContext::GetWorkerNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_node_;
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

}  // namespace execution_context
}  // namespace performance_manager
