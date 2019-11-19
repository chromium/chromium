// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_impl.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"

namespace performance_manager {

namespace {

// Singleton instance of PerformanceManagerImpl. Set from
// PerformanceManagerImpl::StartImpl() and reset from the destructor of
// PerformanceManagerImpl (PM sequence). Accesses should be on the PM sequence.
PerformanceManagerImpl* g_performance_manager_from_pm_sequence = nullptr;

// Singleton instance of PerformanceManagerImpl. Set from Create() and reset
// from Destroy() (external sequence). Accesses can be on any sequence but must
// not race with Create() or Destroy().
//
// TODO(https://crbug.com/1013127): Get rid of
// PerformanceManagerImpl::GetInstance(). Callers can probably use
// PerformanceManagerImpl::CallOnGraphImpl().
PerformanceManagerImpl* g_performance_manager_from_any_sequence = nullptr;

// The performance manager TaskRunner.
base::LazySequencedTaskRunner g_performance_manager_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                         base::MayBlock()));

}  // namespace

PerformanceManagerImpl::~PerformanceManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(g_performance_manager_from_pm_sequence, this);
  // TODO(https://crbug.com/966840): Move this to a TearDown function.
  graph_.TearDown();
  g_performance_manager_from_pm_sequence = nullptr;
}

// static
void PerformanceManagerImpl::CallOnGraphImpl(const base::Location& from_here,
                                             GraphImplCallback callback) {
  DCHECK(callback);
  GetTaskRunner()->PostTask(
      from_here,
      base::BindOnce(&PerformanceManagerImpl::RunCallbackWithGraphImpl,
                     std::move(callback)));
}

PerformanceManagerImpl* PerformanceManagerImpl::GetInstance() {
  return g_performance_manager_from_any_sequence;
}

// static
std::unique_ptr<PerformanceManagerImpl> PerformanceManagerImpl::Create(
    GraphImplCallback on_start) {
  DCHECK(!g_performance_manager_from_any_sequence);

  std::unique_ptr<PerformanceManagerImpl> instance =
      base::WrapUnique(new PerformanceManagerImpl());

  g_performance_manager_from_any_sequence = instance.get();

  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PerformanceManagerImpl::OnStartImpl,
                     base::Unretained(instance.get()), std::move(on_start)));

  return instance;
}

// static
void PerformanceManagerImpl::Destroy(
    std::unique_ptr<PerformanceManagerImpl> instance) {
  DCHECK_EQ(instance.get(), g_performance_manager_from_any_sequence);
  g_performance_manager_from_any_sequence = nullptr;
  GetTaskRunner()->DeleteSoon(FROM_HERE, instance.release());
}

std::unique_ptr<FrameNodeImpl> PerformanceManagerImpl::CreateFrameNode(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    int frame_tree_node_id,
    int render_frame_id,
    const base::UnguessableToken& dev_tools_token,
    int32_t browsing_instance_id,
    int32_t site_instance_id,
    FrameNodeCreationCallback creation_callback) {
  return CreateNodeImpl<FrameNodeImpl>(
      std::move(creation_callback), process_node, page_node, parent_frame_node,
      frame_tree_node_id, render_frame_id, dev_tools_token,
      browsing_instance_id, site_instance_id);
}

std::unique_ptr<PageNodeImpl> PerformanceManagerImpl::CreatePageNode(
    const WebContentsProxy& contents_proxy,
    const std::string& browser_context_id,
    const GURL& visible_url,
    bool is_visible,
    bool is_audible) {
  return CreateNodeImpl<PageNodeImpl>(base::OnceCallback<void(PageNodeImpl*)>(),
                                      contents_proxy, browser_context_id,
                                      visible_url, is_visible, is_audible);
}

std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    RenderProcessHostProxy proxy) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(), proxy);
}

std::unique_ptr<WorkerNodeImpl> PerformanceManagerImpl::CreateWorkerNode(
    const std::string& browser_context_id,
    WorkerNode::WorkerType worker_type,
    ProcessNodeImpl* process_node,
    const GURL& url,
    const base::UnguessableToken& dev_tools_token) {
  return CreateNodeImpl<WorkerNodeImpl>(
      base::OnceCallback<void(WorkerNodeImpl*)>(), browser_context_id,
      worker_type, process_node, url, dev_tools_token);
}

void PerformanceManagerImpl::BatchDeleteNodes(
    std::vector<std::unique_ptr<NodeBase>> nodes) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&PerformanceManagerImpl::BatchDeleteNodesImpl,
                                base::Unretained(this), std::move(nodes)));
}

PerformanceManagerImpl::PerformanceManagerImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

namespace {

// Helper function for adding a node to a graph, and invoking a post-creation
// callback immediately afterwards.
template <typename NodeType>
void AddNodeAndInvokeCreationCallback(
    base::OnceCallback<void(NodeType*)> callback,
    NodeType* node,
    GraphImpl* graph) {
  graph->AddNewNode(node);
  if (callback)
    std::move(callback).Run(node);
}

}  // namespace

template <typename NodeType, typename... Args>
std::unique_ptr<NodeType> PerformanceManagerImpl::CreateNodeImpl(
    base::OnceCallback<void(NodeType*)> creation_callback,
    Args&&... constructor_args) {
  std::unique_ptr<NodeType> new_node = std::make_unique<NodeType>(
      &graph_, std::forward<Args>(constructor_args)...);
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AddNodeAndInvokeCreationCallback<NodeType>,
                                std::move(creation_callback),
                                base::Unretained(new_node.get()),
                                base::Unretained(&graph_)));
  return new_node;
}

void PerformanceManagerImpl::PostDeleteNode(std::unique_ptr<NodeBase> node) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&PerformanceManagerImpl::DeleteNodeImpl,
                                base::Unretained(this), std::move(node)));
}

void PerformanceManagerImpl::DeleteNodeImpl(std::unique_ptr<NodeBase> node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  graph_.RemoveNode(node.get());
}

namespace {

void RemoveFrameAndChildrenFromGraph(FrameNodeImpl* frame_node) {
  // Recurse on the first child while there is one.
  while (!frame_node->child_frame_nodes().empty())
    RemoveFrameAndChildrenFromGraph(*(frame_node->child_frame_nodes().begin()));

  // Now that all children are deleted, delete this frame.
  frame_node->graph()->RemoveNode(frame_node);
}

}  // namespace

void PerformanceManagerImpl::BatchDeleteNodesImpl(
    std::vector<std::unique_ptr<NodeBase>> nodes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_set<ProcessNodeImpl*> process_nodes;

  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    switch ((*it)->type()) {
      case PageNodeImpl::Type(): {
        auto* page_node = PageNodeImpl::FromNodeBase(it->get());

        // Delete the main frame nodes until no more exist.
        while (!page_node->main_frame_nodes().empty())
          RemoveFrameAndChildrenFromGraph(
              *(page_node->main_frame_nodes().begin()));

        graph_.RemoveNode(page_node);
        break;
      }
      case ProcessNodeImpl::Type(): {
        // Keep track of the process nodes for removing once all frames nodes
        // are removed.
        auto* process_node = ProcessNodeImpl::FromNodeBase(it->get());
        process_nodes.insert(process_node);
        break;
      }
      case FrameNodeImpl::Type():
        break;
      case WorkerNodeImpl::Type(): {
        auto* worker_node = WorkerNodeImpl::FromNodeBase(it->get());
        graph_.RemoveNode(worker_node);
        break;
      }
      case SystemNodeImpl::Type():
      case NodeTypeEnum::kInvalidType:
      default: {
        NOTREACHED();
        break;
      }
    }
  }

  // Remove the process nodes from the graph.
  for (auto* process_node : process_nodes)
    graph_.RemoveNode(process_node);

  // When |nodes| goes out of scope, all nodes are deleted.
}

// static
scoped_refptr<base::SequencedTaskRunner>
PerformanceManagerImpl::GetTaskRunner() {
  return g_performance_manager_task_runner.Get();
}

// static
void PerformanceManagerImpl::RunCallbackWithGraphImpl(
    GraphImplCallback graph_callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager_from_pm_sequence) {
    std::move(graph_callback)
        .Run(&g_performance_manager_from_pm_sequence->graph_);
  }
}

// static
void PerformanceManagerImpl::RunCallbackWithGraph(
    GraphCallback graph_callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager_from_pm_sequence) {
    std::move(graph_callback)
        .Run(&g_performance_manager_from_pm_sequence->graph_);
  }
}

void PerformanceManagerImpl::OnStartImpl(GraphImplCallback on_start) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!g_performance_manager_from_pm_sequence);

  g_performance_manager_from_pm_sequence = this;
  graph_.set_ukm_recorder(ukm::UkmRecorder::Get());
  std::move(on_start).Run(&graph_);
}

}  // namespace performance_manager
