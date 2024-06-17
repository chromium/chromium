// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browsing_instance_id.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/process_type.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace content {
class WebContents;
}

namespace url {
class Origin;
}

namespace performance_manager {

struct BrowserProcessNodeTag;

// The performance manager is a rendezvous point for binding to performance
// manager interfaces.
class PerformanceManagerImpl : public PerformanceManager {
 public:
  using FrameNodeCreationCallback = base::OnceCallback<void(FrameNodeImpl*)>;

  PerformanceManagerImpl(const PerformanceManagerImpl&) = delete;
  PerformanceManagerImpl& operator=(const PerformanceManagerImpl&) = delete;

  ~PerformanceManagerImpl() override;

  // Posts a callback that will run on the PM sequence. Valid to call from any
  // sequence.
  //
  // Note: If called from the main thread, the |graph_callback| is guaranteed to
  //       run if and only if "IsAvailable()" returns true.
  //
  //       If called from any other sequence, there is no guarantee that the
  //       callback will run. It will depend on if the PerformanceManager was
  //       destroyed before the the task is scheduled.
  static void CallOnGraphImpl(const base::Location& from_here,
                              base::OnceClosure callback);

  // Same as the above, but the callback is provided a pointer to the graph.
  using GraphImplCallback = base::OnceCallback<void(GraphImpl*)>;
  static void CallOnGraphImpl(const base::Location& from_here,
                              GraphImplCallback graph_callback);

  // Posts a callback that will run on the PM sequence, and be provided a
  // pointer to the Graph. The return value is returned as an argument to the
  // reply callback. As opposed to CallOnGraphImpl(), this is valid to call from
  // the main thread only, and only if "IsAvailable" returns true.
  template <typename TaskReturnType>
  static void CallOnGraphAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<TaskReturnType(GraphImpl*)> task,
      base::OnceCallback<void(TaskReturnType)> reply);

  // Creates, initializes and registers an instance. Invokes |on_start| on the
  // PM sequence. Valid to call from the main thread only.
  static std::unique_ptr<PerformanceManagerImpl> Create(
      GraphImplCallback on_start);

  // Unregisters |instance| and arranges for its deletion on its sequence. Valid
  // to call from the main thread only.
  static void Destroy(std::unique_ptr<PerformanceManager> instance);

  // Creates a new node of the requested type and adds it to the graph.
  // May be called from any sequence. If a |creation_callback| is provided, it
  // will be run on the performance manager sequence immediately after adding
  // the node to the graph. This callback will not be executed if the node could
  // not be added to the graph.
  //
  // Note: If called from the main thread, the node is guaranteed to be added to
  //       the graph if and only if "IsAvailable()" returns true.
  //
  //       If called from any other sequence, there is no guarantee that the
  //       node will be added to the graph. It will depend on if the
  //       PerformanceManager was destroyed before the the task is scheduled.
  static std::unique_ptr<FrameNodeImpl> CreateFrameNode(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node,
      FrameNodeImpl* outer_document_for_fenced_frame,
      int render_frame_id,
      const blink::LocalFrameToken& frame_token,
      content::BrowsingInstanceId browsing_instance_id,
      content::SiteInstanceGroupId site_instance_group_id,
      bool is_current,
      FrameNodeCreationCallback creation_callback =
          FrameNodeCreationCallback());
  static std::unique_ptr<PageNodeImpl> CreatePageNode(
      base::WeakPtr<content::WebContents> web_contents,
      const std::string& browser_context_id,
      const GURL& visible_url,
      PagePropertyFlags initial_properties,
      base::TimeTicks visibility_change_time);
  static std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      BrowserProcessNodeTag tag);
  static std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      RenderProcessHostProxy proxy,
      base::TaskPriority priority);
  static std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      content::ProcessType process_type,
      BrowserChildProcessHostProxy proxy);
  static std::unique_ptr<WorkerNodeImpl> CreateWorkerNode(
      const std::string& browser_context_id,
      WorkerNode::WorkerType worker_type,
      ProcessNodeImpl* process_node,
      const blink::WorkerToken& worker_token,
      const url::Origin& origin);

  // Destroys a node returned from the creation functions above. May be called
  // from any sequence.
  static void DeleteNode(std::unique_ptr<NodeBase> node);

  // Destroys multiples nodes in one single task. Equivalent to calling
  // DeleteNode() on all elements of the vector. This function takes care of
  // removing them from the graph in topological order and destroying them.
  // May be called from any sequence.
  static void BatchDeleteNodes(std::vector<std::unique_ptr<NodeBase>> nodes);

  // Indicates whether or not the caller is currently running on the PM task
  // runner.
  static bool OnPMTaskRunnerForTesting();

  // Allows testing code to know when tear down is complete. This can only be
  // called from the main thread, and the callback will also be invoked on the
  // main thread.
  static void SetOnDestroyedCallbackForTesting(base::OnceClosure callback);

 private:
  friend class PerformanceManager;

  PerformanceManagerImpl();

  // Returns the performance manager TaskRunner.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Retrieves the currently registered instance. Can only be called from the PM
  // sequence.
  // Note: Only exists so that RunCallbackWithGraphAndReplyWithResult can be
  // implemented in the header file.
  static PerformanceManagerImpl* GetInstance();

  template <typename NodeType, typename... Args>
  static std::unique_ptr<NodeType> CreateNodeImpl(
      base::OnceCallback<void(NodeType*)> creation_callback,
      Args&&... constructor_args);

  // Helper functions that removes a node/vector of nodes from the graph on the
  // PM sequence and deletes them.
  //
  // Note that this function has similar semantics to
  // SequencedTaskRunner::DeleteSoon(). The node/vector of nodes is passed via a
  // regular pointer so that they are not deleted if the task is not executed.
  static void DeleteNodeImpl(NodeBase* node_ptr, GraphImpl* graph);
  static void BatchDeleteNodesImpl(
      std::vector<std::unique_ptr<NodeBase>>* nodes_ptr,
      GraphImpl* graph);

  void OnStartImpl(GraphImplCallback graph_callback);
  static void RunCallbackWithGraphImpl(GraphImplCallback graph_callback);
  static void RunCallbackWithGraph(GraphCallback graph_callback);

  template <typename TaskReturnType>
  static TaskReturnType RunCallbackWithGraphAndReplyWithResult(
      base::OnceCallback<TaskReturnType(GraphImpl*)> task);

  static void SetOnDestroyedCallbackImpl(base::OnceClosure callback);

  GraphImpl graph_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosure on_destroyed_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If the PM is running on the UI sequence, this is its task runner.
  // Otherwise it uses a thread pool task runner defined in the .cc file.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// static
template <typename TaskReturnType>
void PerformanceManagerImpl::CallOnGraphAndReplyWithResult(
    const base::Location& from_here,
    base::OnceCallback<TaskReturnType(GraphImpl*)> task,
    base::OnceCallback<void(TaskReturnType)> reply) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      from_here,
      base::BindOnce(
          &PerformanceManagerImpl::RunCallbackWithGraphAndReplyWithResult<
              TaskReturnType>,
          std::move(task)),
      std::move(reply));
}

// static
template <typename TaskReturnType>
TaskReturnType PerformanceManagerImpl::RunCallbackWithGraphAndReplyWithResult(
    base::OnceCallback<TaskReturnType(GraphImpl*)> task) {
  return std::move(task).Run(&GetInstance()->graph_);
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
