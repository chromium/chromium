// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/public/web_contents_proxy.h"

class GURL;

namespace performance_manager {

class PageNodeImpl;

// The performance manager is a rendezvous point for binding to performance
// manager interfaces.
class PerformanceManagerImpl : public PerformanceManager {
 public:
  using FrameNodeCreationCallback = base::OnceCallback<void(FrameNodeImpl*)>;

  ~PerformanceManagerImpl() override;

  // Posts a callback that will run on the PM sequence, and be provided a
  // pointer to the Graph. Valid to call from any sequence, but |graph_callback|
  // won't run if this is called before Create() or after Destroy().
  //
  // TODO(chrisha): Move this to the public interface.
  using GraphImplCallback = base::OnceCallback<void(GraphImpl*)>;
  static void CallOnGraphImpl(const base::Location& from_here,
                              GraphImplCallback graph_callback);

  // Posts a callback that will run on the PM sequence, and be provided a
  // pointer to the Graph. Valid to be called from the main thread only, and
  // only if "IsAvailable" returns true. The return value is returned as an
  // argument to the reply callback.
  template <typename TaskReturnType>
  void CallOnGraphAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<TaskReturnType(GraphImpl*)> task,
      base::OnceCallback<void(TaskReturnType)> reply);

  // Retrieves the currently registered instance. Calls must not race with
  // Create() or Destroy(). The returned pointer must not be used after
  // Destroy(). This function can be called from any sequence with those
  // caveats.
  static PerformanceManagerImpl* GetInstance();

  // Creates, initializes and registers an instance.
  // Invokes |on_start| on the PM sequence.
  static std::unique_ptr<PerformanceManagerImpl> Create(
      GraphImplCallback on_start);

  // Unregisters |instance| if it's currently registered and arranges for its
  // deletion on its sequence.
  static void Destroy(std::unique_ptr<PerformanceManagerImpl> instance);

  // Creates a new node of the requested type and adds it to the graph.
  // May be called from any sequence. If a |creation_callback| is provided it
  // will be run on the performance manager sequence immediately after creating
  // the node.
  std::unique_ptr<FrameNodeImpl> CreateFrameNode(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node,
      int frame_tree_node_id,
      int render_frame_id,
      const base::UnguessableToken& dev_tools_token,
      int32_t browsing_instance_id,
      int32_t site_instance_id,
      FrameNodeCreationCallback creation_callback =
          FrameNodeCreationCallback());
  std::unique_ptr<PageNodeImpl> CreatePageNode(
      const WebContentsProxy& contents_proxy,
      const std::string& browser_context_id,
      const GURL& visible_url,
      bool is_visible,
      bool is_audible);
  std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      RenderProcessHostProxy proxy);
  std::unique_ptr<WorkerNodeImpl> CreateWorkerNode(
      const std::string& browser_context_id,
      WorkerNode::WorkerType worker_type,
      ProcessNodeImpl* process_node,
      const GURL& url,
      const base::UnguessableToken& dev_tools_token);

  // Destroys a node returned from the creation functions above.
  // May be called from any sequence.
  template <typename NodeType>
  void DeleteNode(std::unique_ptr<NodeType> node);

  // Each node in |nodes| must have been returned from one of the creation
  // functions above. This function takes care of removing them from the graph
  // in topological order and destroying them.
  void BatchDeleteNodes(std::vector<std::unique_ptr<NodeBase>> nodes);

  // Returns the performance manager TaskRunner.
  // TODO(chrisha): Hide this after the last consumer stops using it!
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Indicates whether or not the caller is currently running on the PM task
  // runner.
  bool OnPMTaskRunnerForTesting() const {
    return GetTaskRunner()->RunsTasksInCurrentSequence();
  }

 private:
  friend class PerformanceManager;

  PerformanceManagerImpl();

  template <typename NodeType, typename... Args>
  std::unique_ptr<NodeType> CreateNodeImpl(
      base::OnceCallback<void(NodeType*)> creation_callback,
      Args&&... constructor_args);

  void PostDeleteNode(std::unique_ptr<NodeBase> node);
  void DeleteNodeImpl(std::unique_ptr<NodeBase> node);
  void BatchDeleteNodesImpl(std::vector<std::unique_ptr<NodeBase>> nodes);

  void OnStartImpl(GraphImplCallback graph_callback);
  static void RunCallbackWithGraphImpl(GraphImplCallback graph_callback);
  static void RunCallbackWithGraph(GraphCallback graph_callback);

  template <typename TaskReturnType>
  TaskReturnType RunCallbackWithGraphAndReplyWithResult(
      base::OnceCallback<TaskReturnType(GraphImpl*)> task);

  GraphImpl graph_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerImpl);
};

template <typename NodeType>
void PerformanceManagerImpl::DeleteNode(std::unique_ptr<NodeType> node) {
  PostDeleteNode(std::move(node));
}

template <typename TaskReturnType>
void PerformanceManagerImpl::CallOnGraphAndReplyWithResult(
    const base::Location& from_here,
    base::OnceCallback<TaskReturnType(GraphImpl*)> task,
    base::OnceCallback<void(TaskReturnType)> reply) {
  auto* pm = GetInstance();
  base::PostTaskAndReplyWithResult(
      GetTaskRunner().get(), from_here,
      base::BindOnce(
          &PerformanceManagerImpl::RunCallbackWithGraphAndReplyWithResult<
              TaskReturnType>,
          base::Unretained(pm), std::move(task)),
      std::move(reply));
}

template <typename TaskReturnType>
TaskReturnType PerformanceManagerImpl::RunCallbackWithGraphAndReplyWithResult(
    base::OnceCallback<TaskReturnType(GraphImpl*)> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::move(task).Run(&graph_);
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
