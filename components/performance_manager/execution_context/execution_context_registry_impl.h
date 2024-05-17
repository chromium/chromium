// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_REGISTRY_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_REGISTRY_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
namespace execution_context {

class ExecutionContext;

// The ExecutionContextRegistry is a GraphRegistered class that allows for
// observers to be registered, and for ExecutionContexts to be looked up by
// their tokens. SetUp() must be called prior to any nodes being created.
class ExecutionContextRegistryImpl
    : public ExecutionContextRegistry,
      public GraphRegisteredImpl<ExecutionContextRegistryImpl>,
      public FrameNode::ObserverDefaultImpl,
      public WorkerNode::ObserverDefaultImpl {
 public:
  ExecutionContextRegistryImpl();
  ExecutionContextRegistryImpl(const ExecutionContextRegistryImpl&) = delete;
  ExecutionContextRegistryImpl& operator=(const ExecutionContextRegistryImpl&) =
      delete;
  ~ExecutionContextRegistryImpl() override;

  // Sets up/tears down the instance on the graph.
  void SetUp(Graph* graph);
  void TearDown(Graph* graph);

  // ExecutionContextRegistry implementation:
  void AddObserver(ExecutionContextObserver* observer) override;
  bool HasObserver(ExecutionContextObserver* observer) const override;
  void RemoveObserver(ExecutionContextObserver* observer) override;
  const ExecutionContext* GetExecutionContextByToken(
      const blink::ExecutionContextToken& token) override;
  const FrameNode* GetFrameNodeByFrameToken(
      const blink::LocalFrameToken& token) override;
  const WorkerNode* GetWorkerNodeByWorkerToken(
      const blink::WorkerToken& token) override;
  const ExecutionContext* GetExecutionContextForFrameNodeImpl(
      const FrameNode* frame_node) override;
  const ExecutionContext* GetExecutionContextForWorkerNodeImpl(
      const WorkerNode* worker_node) override;

  size_t GetExecutionContextCountForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return execution_contexts_.size();
  }

 private:
  // FrameNode::ObserverDefaultImpl implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) override;

  // WorkerNode::ObserverDefaultImpl implementation:
  void OnWorkerNodeAdded(const WorkerNode* worker_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;
  void OnPriorityAndReasonChanged(
      const WorkerNode* worker_node,
      const PriorityAndReason& previous_value) override;

  // Maintains the collection of all currently known ExecutionContexts in the
  // Graph. It is expected that there are O(100s) to O(1000s) of these being
  // tracked. This is stored as a hash set keyed by the token, so that the
  // token itself doesn't have to be duplicated as would be the case with a map.
  // TODO: Whenever C++20 is supported, make these transparent!
  struct ExecutionContextHash {
    size_t operator()(const ExecutionContext* ec) const;
  };
  struct ExecutionContextKeyEqual {
    bool operator()(const ExecutionContext* ec1,
                    const ExecutionContext* ec2) const;
  };
  std::unordered_set<raw_ptr<const ExecutionContext, CtnExperimental>,
                     ExecutionContextHash,
                     ExecutionContextKeyEqual>
      execution_contexts_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ObserverList<ExecutionContextObserver,
                     /* check_empty = */ true,
                     /* allow_reentrancy */ false>
      observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace execution_context
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_REGISTRY_IMPL_H_
