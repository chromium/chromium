// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context/execution_context_registry_impl.h"

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "components/performance_manager/execution_context/execution_context_impl.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "url/gurl.h"

namespace performance_manager {
namespace execution_context {

namespace {

// std::unordered_set doesn't support transparent keys until C++20, so we use
// a custom ExecutionContext wrapper for the time being.
class DummyExecutionContextForLookup : public ExecutionContext {
 public:
  explicit DummyExecutionContextForLookup(
      const blink::ExecutionContextToken& token)
      : token_(token) {}
  DummyExecutionContextForLookup(const DummyExecutionContextForLookup&) =
      delete;
  DummyExecutionContextForLookup& operator=(
      const DummyExecutionContextForLookup&) = delete;
  ~DummyExecutionContextForLookup() override = default;

  // ExecutionContext implementation:

  ExecutionContextType GetType() const override { NOTREACHED(); }

  blink::ExecutionContextToken GetToken() const override { return *token_; }

  Graph* GetGraph() const override { NOTREACHED(); }

  const GURL& GetUrl() const override { NOTREACHED(); }

  const ProcessNode* GetProcessNode() const override { NOTREACHED(); }

  const PriorityAndReason& GetPriorityAndReason() const override {
    NOTREACHED();
  }

  const FrameNode* GetFrameNode() const override { NOTREACHED(); }

  const WorkerNode* GetWorkerNode() const override { NOTREACHED(); }

 private:
  const raw_ref<const blink::ExecutionContextToken> token_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextRegistry

ExecutionContextRegistry::ExecutionContextRegistry() = default;

ExecutionContextRegistry::~ExecutionContextRegistry() = default;

// static
ExecutionContextRegistry* ExecutionContextRegistry::GetFromGraph(Graph* graph) {
  return GraphRegisteredImpl<ExecutionContextRegistryImpl>::GetFromGraph(graph);
}

// static
const ExecutionContext*
ExecutionContextRegistry::GetExecutionContextForFrameNode(
    const FrameNode* frame_node) {
  auto* ec_registry = GetFromGraph(frame_node->GetGraph());
  DCHECK(ec_registry);
  return ec_registry->GetExecutionContextForFrameNodeImpl(frame_node);
}

// static
const ExecutionContext*
ExecutionContextRegistry::GetExecutionContextForWorkerNode(
    const WorkerNode* worker_node) {
  auto* ec_registry = GetFromGraph(worker_node->GetGraph());
  DCHECK(ec_registry);
  return ec_registry->GetExecutionContextForWorkerNodeImpl(worker_node);
}

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextRegistryImpl

ExecutionContextRegistryImpl::ExecutionContextRegistryImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ExecutionContextRegistryImpl::~ExecutionContextRegistryImpl() = default;

void ExecutionContextRegistryImpl::AddObserver(
    ExecutionContextObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

bool ExecutionContextRegistryImpl::HasObserver(
    ExecutionContextObserver* observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observers_.HasObserver(observer);
}

void ExecutionContextRegistryImpl::RemoveObserver(
    ExecutionContextObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

const ExecutionContext*
ExecutionContextRegistryImpl::GetExecutionContextByToken(
    const blink::ExecutionContextToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (token.value().is_empty())
    return nullptr;
  DummyExecutionContextForLookup key(token);
  auto it = execution_contexts_.find(&key);
  if (it == execution_contexts_.end())
    return nullptr;
  return *it;
}

const FrameNode* ExecutionContextRegistryImpl::GetFrameNodeByFrameToken(
    const blink::LocalFrameToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextByToken(token);
  if (!ec)
    return nullptr;
  return ec->GetFrameNode();
}

const WorkerNode* ExecutionContextRegistryImpl::GetWorkerNodeByWorkerToken(
    const blink::WorkerToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextByToken(blink::ExecutionContextToken(token));
  if (!ec)
    return nullptr;
  return ec->GetWorkerNode();
}

const ExecutionContext*
ExecutionContextRegistryImpl::GetExecutionContextForFrameNodeImpl(
    const FrameNode* frame_node) {
  return &FrameExecutionContext::Get(FrameNodeImpl::FromNode(frame_node));
}

const ExecutionContext*
ExecutionContextRegistryImpl::GetExecutionContextForWorkerNodeImpl(
    const WorkerNode* worker_node) {
  return &WorkerExecutionContext::Get(WorkerNodeImpl::FromNode(worker_node));
}

void ExecutionContextRegistryImpl::SetUp(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph->HasOnlySystemNode());
  graph->RegisterObject(this);
  graph->AddFrameNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void ExecutionContextRegistryImpl::TearDown(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveWorkerNodeObserver(this);
  graph->RemoveFrameNodeObserver(this);
  graph->UnregisterObject(this);
}

void ExecutionContextRegistryImpl::OnFrameNodeAdded(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextForFrameNodeImpl(frame_node);
  DCHECK(ec);
  auto result = execution_contexts_.insert(ec);
  DCHECK(result.second);  // Inserted.
  for (auto& observer : observers_)
    observer.OnExecutionContextAdded(ec);
}

void ExecutionContextRegistryImpl::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextForFrameNodeImpl(frame_node);
  DCHECK(ec);
  for (auto& observer : observers_)
    observer.OnBeforeExecutionContextRemoved(ec);
  size_t erased = execution_contexts_.erase(ec);
  DCHECK_EQ(1u, erased);
}

void ExecutionContextRegistryImpl::OnPriorityAndReasonChanged(
    const FrameNode* frame_node,
    const PriorityAndReason& previous_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextForFrameNodeImpl(frame_node);
  DCHECK(ec);
  for (auto& observer : observers_)
    observer.OnPriorityAndReasonChanged(ec, previous_value);
}

void ExecutionContextRegistryImpl::OnWorkerNodeAdded(
    const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextForWorkerNodeImpl(worker_node);
  DCHECK(ec);

  auto result = execution_contexts_.insert(ec);
  DCHECK(result.second);  // Inserted.

  for (auto& observer : observers_)
    observer.OnExecutionContextAdded(ec);
}

void ExecutionContextRegistryImpl::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextForWorkerNodeImpl(worker_node);
  DCHECK(ec);
  for (auto& observer : observers_)
    observer.OnBeforeExecutionContextRemoved(ec);

  size_t erased = execution_contexts_.erase(ec);
  DCHECK_EQ(1u, erased);
}

void ExecutionContextRegistryImpl::OnPriorityAndReasonChanged(
    const WorkerNode* worker_node,
    const PriorityAndReason& previous_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ec = GetExecutionContextForWorkerNodeImpl(worker_node);
  DCHECK(ec);
  for (auto& observer : observers_)
    observer.OnPriorityAndReasonChanged(ec, previous_value);
}

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextRegistryImpl::ExecutionContextHash

size_t ExecutionContextRegistryImpl::ExecutionContextHash::operator()(
    const ExecutionContext* ec) const {
  return base::UnguessableTokenHash()(ec->GetToken().value());
}

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextRegistryImpl::ExecutionContextKeyEqual

bool ExecutionContextRegistryImpl::ExecutionContextKeyEqual::operator()(
    const ExecutionContext* ec1,
    const ExecutionContext* ec2) const {
  return ec1->GetToken() == ec2->GetToken();
}

}  // namespace execution_context
}  // namespace performance_manager
