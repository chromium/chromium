// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_impl.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {

// Singleton instance of PerformanceManagerImpl. Set from
// PerformanceManagerImpl::StartImpl() and reset from the destructor of
// PerformanceManagerImpl (PM sequence). Should only be accessed on the PM
// sequence.
PerformanceManagerImpl* g_performance_manager = nullptr;

// Indicates if a task posted to `GetTaskRunner()` will have
// access to a valid PerformanceManagerImpl instance via
// |g_performance_manager|. Should only be accessed on the main thread.
bool g_pm_is_available = false;

constexpr base::TaskPriority kPmTaskPriority = base::TaskPriority::USER_VISIBLE;

// Task traits appropriate for the PM task runner.
// NOTE: The PM task runner has to block shutdown as some of the tasks posted to
// it should be guaranteed to run before shutdown (e.g. removing some entries
// from the site data store).
constexpr base::TaskTraits kPMTaskTraits = {
    kPmTaskPriority, base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
    base::MayBlock()};

// Builds a UI task runner with the appropriate traits for the PM.
// TODO(crbug.com/1189677): The PM task runner has to block shutdown as some of
// the tasks posted to it should be guaranteed to run before shutdown (e.g.
// removing some entries from the site data store). The UI thread ignores
// MayBlock and TaskShutdownBehavior, so these tasks and any blocking tasks must
// be found and migrated to a worker thread.
scoped_refptr<base::SequencedTaskRunner> GetUITaskRunner() {
  return content::GetUIThreadTaskRunner({kPmTaskPriority});
}

}  // namespace

// static
bool PerformanceManager::IsAvailable() {
  return g_pm_is_available;
}

PerformanceManagerImpl::~PerformanceManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(g_performance_manager, this);
  // TODO(https://crbug.com/966840): Move this to a TearDown function.
  graph_.TearDown();
  g_performance_manager = nullptr;
  if (on_destroyed_callback_)
    std::move(on_destroyed_callback_).Run();
}

// static
void PerformanceManagerImpl::CallOnGraphImpl(const base::Location& from_here,
                                             base::OnceClosure callback) {
  DCHECK(callback);
  GetTaskRunner()->PostTask(from_here, std::move(callback));
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

// static
std::unique_ptr<PerformanceManagerImpl> PerformanceManagerImpl::Create(
    GraphImplCallback on_start) {
  DCHECK(!g_pm_is_available);
  g_pm_is_available = true;

  std::unique_ptr<PerformanceManagerImpl> instance =
      base::WrapUnique(new PerformanceManagerImpl());

  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PerformanceManagerImpl::OnStartImpl,
                     base::Unretained(instance.get()), std::move(on_start)));

  return instance;
}

// static
void PerformanceManagerImpl::Destroy(
    std::unique_ptr<PerformanceManager> instance) {
  DCHECK(g_pm_is_available);
  g_pm_is_available = false;
  GetTaskRunner()->DeleteSoon(FROM_HERE, instance.release());
}

// static
std::unique_ptr<FrameNodeImpl> PerformanceManagerImpl::CreateFrameNode(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    int render_frame_id,
    const blink::LocalFrameToken& frame_token,
    content::BrowsingInstanceId browsing_instance_id,
    content::SiteInstanceId site_instance_id,
    FrameNodeCreationCallback creation_callback) {
  return CreateNodeImpl<FrameNodeImpl>(
      std::move(creation_callback), process_node, page_node, parent_frame_node,
      render_frame_id, frame_token, browsing_instance_id, site_instance_id);
}

// static
std::unique_ptr<PageNodeImpl> PerformanceManagerImpl::CreatePageNode(
    const WebContentsProxy& contents_proxy,
    const std::string& browser_context_id,
    const GURL& visible_url,
    bool is_visible,
    bool is_audible,
    base::TimeTicks visibility_change_time,
    PageNode::PageState page_state) {
  return CreateNodeImpl<PageNodeImpl>(base::OnceCallback<void(PageNodeImpl*)>(),
                                      contents_proxy, browser_context_id,
                                      visible_url, is_visible, is_audible,
                                      visibility_change_time, page_state);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    BrowserProcessNodeTag tag) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(), tag);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    RenderProcessHostProxy render_process_host_proxy) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(),
      std::move(render_process_host_proxy));
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    content::ProcessType process_type,
    BrowserChildProcessHostProxy browser_child_process_host_proxy) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(), process_type,
      std::move(browser_child_process_host_proxy));
}

// static
std::unique_ptr<WorkerNodeImpl> PerformanceManagerImpl::CreateWorkerNode(
    const std::string& browser_context_id,
    WorkerNode::WorkerType worker_type,
    ProcessNodeImpl* process_node,
    const blink::WorkerToken& worker_token) {
  return CreateNodeImpl<WorkerNodeImpl>(
      base::OnceCallback<void(WorkerNodeImpl*)>(), browser_context_id,
      worker_type, process_node, worker_token);
}

// static
void PerformanceManagerImpl::DeleteNode(std::unique_ptr<NodeBase> node) {
  CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PerformanceManagerImpl::DeleteNodeImpl, node.release()));
}

// static
void PerformanceManagerImpl::BatchDeleteNodes(
    std::vector<std::unique_ptr<NodeBase>> nodes) {
  // Move the nodes vector to the heap.
  auto nodes_ptr = std::make_unique<std::vector<std::unique_ptr<NodeBase>>>(
      std::move(nodes));
  CallOnGraphImpl(FROM_HERE,
                  base::BindOnce(&PerformanceManagerImpl::BatchDeleteNodesImpl,
                                 nodes_ptr.release()));
}

// static
bool PerformanceManagerImpl::OnPMTaskRunnerForTesting() {
  return GetTaskRunner()->RunsTasksInCurrentSequence();
}

// static
void PerformanceManagerImpl::SetOnDestroyedCallbackForTesting(
    base::OnceClosure callback) {
  // Bind the callback in one that can be called on the PM sequence (it also
  // binds the main thread, and bounces a task back to that thread).
  scoped_refptr<base::SequencedTaskRunner> main_thread =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::OnceClosure pm_callback = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> main_thread,
         base::OnceClosure callback) {
        main_thread->PostTask(FROM_HERE, std::move(callback));
      },
      main_thread, std::move(callback));

  // Pass the callback to the PM.
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PerformanceManagerImpl::SetOnDestroyedCallbackImpl,
                     std::move(pm_callback)));
}

PerformanceManagerImpl::PerformanceManagerImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(features::kRunOnMainThread)) {
    ui_task_runner_ = GetUITaskRunner();
  }
}

// static
scoped_refptr<base::SequencedTaskRunner>
PerformanceManagerImpl::GetTaskRunner() {
  if (base::FeatureList::IsEnabled(features::kRunOnMainThread)) {
    CHECK(!base::FeatureList::IsEnabled(
        features::kRunOnDedicatedThreadPoolThread));
    // Used the cached runner, if available. This prevents doing repeated
    // lookups.
    if (g_performance_manager)
      return g_performance_manager->ui_task_runner_;
    // Our semantics are that this always returns a valid task runner as long
    // as there is a task environment alive. We can't cache this in a local
    // static variable because it will become invalid across test boundaries.
    // Note that this doesn't result in a new task runner being created; it
    // simply causes a table lookup to find the existing task runner with the
    // appropriate type, which will be the same task runner that was cached by
    // |g_performance_manager| while it was alive.
    return GetUITaskRunner();
  }
  if (base::FeatureList::IsEnabled(features::kRunOnDedicatedThreadPoolThread)) {
    CHECK(!base::FeatureList::IsEnabled(features::kRunOnMainThread));
    // Use a dedicated thread so that all tasks on the PM sequence can be
    // identified in traces.
    static base::LazyThreadPoolSingleThreadTaskRunner task_runner =
        LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
            kPMTaskTraits, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    return task_runner.Get();
  }
  static base::LazyThreadPoolSequencedTaskRunner
      performance_manager_task_runner =
          LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(kPMTaskTraits);
  return performance_manager_task_runner.Get();
}

PerformanceManagerImpl* PerformanceManagerImpl::GetInstance() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  return g_performance_manager;
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

// static
template <typename NodeType, typename... Args>
std::unique_ptr<NodeType> PerformanceManagerImpl::CreateNodeImpl(
    base::OnceCallback<void(NodeType*)> creation_callback,
    Args&&... constructor_args) {
  std::unique_ptr<NodeType> new_node =
      std::make_unique<NodeType>(std::forward<Args>(constructor_args)...);
  CallOnGraphImpl(FROM_HERE,
                  base::BindOnce(&AddNodeAndInvokeCreationCallback<NodeType>,
                                 std::move(creation_callback),
                                 base::Unretained(new_node.get())));
  return new_node;
}

// static
void PerformanceManagerImpl::DeleteNodeImpl(NodeBase* node_ptr,
                                            GraphImpl* graph) {
  // Must be done first to avoid leaking |node_ptr|.
  std::unique_ptr<NodeBase> node(node_ptr);

  graph->RemoveNode(node.get());
}

namespace {

void RemoveFrameAndChildrenFromGraph(FrameNodeImpl* frame_node,
                                     GraphImpl* graph) {
  // Recurse on the first child while there is one.
  while (!frame_node->child_frame_nodes().empty()) {
    RemoveFrameAndChildrenFromGraph(*(frame_node->child_frame_nodes().begin()),
                                    graph);
  }

  // Now that all children are deleted, delete this frame.
  graph->RemoveNode(frame_node);
}

}  // namespace

// static
void PerformanceManagerImpl::BatchDeleteNodesImpl(
    std::vector<std::unique_ptr<NodeBase>>* nodes_ptr,
    GraphImpl* graph) {
  // Must be done first to avoid leaking |nodes_ptr|.
  std::unique_ptr<std::vector<std::unique_ptr<NodeBase>>> nodes(nodes_ptr);

  base::flat_set<ProcessNodeImpl*> process_nodes;

  for (const auto& node : *nodes) {
    switch (node->type()) {
      case PageNodeImpl::Type(): {
        auto* page_node = PageNodeImpl::FromNodeBase(node.get());

        // Delete the main frame nodes until no more exist.
        while (!page_node->main_frame_nodes().empty()) {
          RemoveFrameAndChildrenFromGraph(
              *(page_node->main_frame_nodes().begin()), graph);
        }

        graph->RemoveNode(page_node);
        break;
      }
      case ProcessNodeImpl::Type(): {
        // Keep track of the process nodes for removing once all frames nodes
        // are removed.
        auto* process_node = ProcessNodeImpl::FromNodeBase(node.get());
        process_nodes.insert(process_node);
        break;
      }
      case FrameNodeImpl::Type():
        break;
      case WorkerNodeImpl::Type(): {
        auto* worker_node = WorkerNodeImpl::FromNodeBase(node.get());
        graph->RemoveNode(worker_node);
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
    graph->RemoveNode(process_node);

  // When |nodes| goes out of scope, all nodes are deleted.
}

void PerformanceManagerImpl::OnStartImpl(GraphImplCallback on_start) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!g_performance_manager);

  if (base::FeatureList::IsEnabled(features::kRunOnDedicatedThreadPoolThread)) {
    // This should be the first task that runs on the dedicated thread.
    base::PlatformThread::SetName("Performance Manager");
  }

  g_performance_manager = this;
  graph_.SetUp();
  graph_.set_ukm_recorder(ukm::UkmRecorder::Get());
  std::move(on_start).Run(&graph_);
}

// static
void PerformanceManagerImpl::RunCallbackWithGraphImpl(
    GraphImplCallback graph_callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager)
    std::move(graph_callback).Run(&g_performance_manager->graph_);
}

// static
void PerformanceManagerImpl::RunCallbackWithGraph(
    GraphCallback graph_callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager)
    std::move(graph_callback).Run(&g_performance_manager->graph_);
}

// static
void PerformanceManagerImpl::SetOnDestroyedCallbackImpl(
    base::OnceClosure callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(g_performance_manager->sequence_checker_);
    g_performance_manager->on_destroyed_callback_ = std::move(callback);
  }
}

}  // namespace performance_manager
