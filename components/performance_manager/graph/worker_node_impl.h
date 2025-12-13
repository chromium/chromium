// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_H_

#include <memory>
#include <string>

#include "base/byte_count.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/execution_context/execution_context_impl.h"
#include "components/performance_manager/graph/node_attached_data_storage.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_data.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

class FrameNodeImpl;
class ProcessNodeImpl;

class WorkerNodeImpl
    : public PublicNodeImpl<WorkerNodeImpl, WorkerNode>,
      public TypedNodeBase<WorkerNodeImpl, WorkerNode, WorkerNodeObserver>,
      public SupportsNodeInlineData<
          execution_context::WorkerExecutionContext,
          resource_attribution::SharedCPUTimeResultData,
          // Keep this last to avoid merge conflicts.
          NodeAttachedDataStorage> {
 public:
  static const char kDefaultPriorityReason[];

  using TypedNodeBase<WorkerNodeImpl, WorkerNode, WorkerNodeObserver>::FromNode;

  WorkerNodeImpl(const std::string& browser_context_id,
                 WorkerType worker_type,
                 ProcessNodeImpl* process_node,
                 const blink::WorkerToken& worker_token,
                 const url::Origin& origin);

  WorkerNodeImpl(const WorkerNodeImpl&) = delete;
  WorkerNodeImpl& operator=(const WorkerNodeImpl&) = delete;

  ~WorkerNodeImpl() override;

  // Partial WorkerNode implementation:
  WorkerType GetWorkerType() const override;
  const std::string& GetBrowserContextID() const override;
  const blink::WorkerToken& GetWorkerToken() const override;
  resource_attribution::WorkerContext GetResourceContext() const override;
  const GURL& GetURL() const override;
  const url::Origin& GetOrigin() const override;
  const PriorityAndReason& GetPriorityAndReason() const override;
  base::ByteCount GetResidentSetEstimate() const override;
  base::ByteCount GetPrivateFootprintEstimate() const override;

  // Invoked when a frame starts/stops being a client of this worker.
  void AddClientFrame(FrameNodeImpl* frame_node);
  void RemoveClientFrame(FrameNodeImpl* frame_node);

  // Invoked when a worker starts/stops being a client of this worker.
  void AddClientWorker(WorkerNodeImpl* worker_node);
  void RemoveClientWorker(WorkerNodeImpl* worker_node);

  // Setters are not thread safe.
  void SetPriorityAndReason(const PriorityAndReason& priority_and_reason);
  void SetResidentSetEstimate(base::ByteCount rss_estimate);
  void SetPrivateFootprintEstimate(base::ByteCount pmf_estimate);

  // Invoked when the worker script was fetched and the final response URL is
  // available.
  void OnFinalResponseURLDetermined(const GURL& url);

  // Getters for const properties.
  ProcessNodeImpl* process_node() const;

  // Getters for non-const properties. These are not thread safe.
  NodeSetView<FrameNodeImpl*> client_frames() const;
  NodeSetView<WorkerNodeImpl*> client_workers() const;
  NodeSetView<WorkerNodeImpl*> child_workers() const;

  base::WeakPtr<WorkerNodeImpl> GetWeakPtr();

 private:
  friend class WorkerNodeImplDescriber;

  // NodeBase:
  void OnInitializingProperties() override;
  void OnInitializingEdges() override;
  void OnUninitializingEdges() override;
  void CleanUpNodeState() override;

  // Rest of WorkerNode implementation. These are private so that users of the
  // impl use the private getters rather than the public interface.
  const ProcessNode* GetProcessNode() const override;
  NodeSetView<const FrameNode*> GetClientFrames() const override;
  NodeSetView<const WorkerNode*> GetClientWorkers() const override;
  NodeSetView<const WorkerNode*> GetChildWorkers() const override;

  // Invoked when |worker_node| becomes a child of this worker.
  void AddChildWorker(WorkerNodeImpl* worker_node);
  void RemoveChildWorker(WorkerNodeImpl* worker_node);

  // The unique ID of the browser context that this worker belongs to.
  const std::string browser_context_id_;

  // The type of this worker.
  const WorkerType worker_type_;

  // The process in which this worker lives.
  const raw_ptr<ProcessNodeImpl> process_node_;

  // A unique identifier shared with all representations of this worker across
  // content and blink. This token should only ever be sent between the browser
  // and the renderer hosting the worker. It should not be used to identify a
  // worker in browser-to-renderer control messages, but may be used to identify
  // a worker in informational messages going in either direction.
  const blink::WorkerToken worker_token_;

  // The URL of the worker script. This is the final response URL which takes
  // into account redirections. This is initially empty and it is set when
  // OnFinalResponseURLDetermined() is invoked.
  GURL url_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The worker's security origin. Set at creation and never varies.
  const url::Origin origin_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Frames that are clients of this worker.
  NodeSet client_frames_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Other workers that are clients of this worker. See the declaration of
  // WorkerNode for a distinction between client workers and child workers.
  NodeSet client_workers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The child workers of this worker. See the declaration of WorkerNode for a
  // distinction between client workers and child workers.
  NodeSet child_workers_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ByteCount resident_set_estimate_;

  base::ByteCount private_footprint_estimate_;

  // Worker priority information. Set via ExecutionContextPriorityDecorator.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      PriorityAndReason,
      &WorkerNodeObserver::OnPriorityAndReasonChanged>
      priority_and_reason_ GUARDED_BY_CONTEXT(sequence_checker_){
          PriorityAndReason(base::TaskPriority::LOWEST,
                            kDefaultPriorityReason)};

  base::WeakPtrFactory<WorkerNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_H_
