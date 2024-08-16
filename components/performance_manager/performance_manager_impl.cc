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
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

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
// TODO(crbug.com/40755583): The PM task runner has to block shutdown as some of
// the tasks posted to it should be guaranteed to run before shutdown (e.g.
// removing some entries from the site data store). The UI thread ignores
// MayBlock and TaskShutdownBehavior, so these tasks and any blocking tasks must
// be found and migrated to a worker thread.
scoped_refptr<base::SequencedTaskRunner> GetUITaskRunner() {
  return content::GetUIThreadTaskRunner({kPmTaskPriority});
}

// A `TaskRunner` which runs callbacks synchronously when they're posted with no
// delay from the UI thread, or posts to the UI thread otherwise.
//
// Note: The UI thread `TaskRunner` is obtained from `GetUITaskRunner()` in each
// method called, rather than being cached in a member, to ensure that the
// correct `TaskRunner` is used across tests that run in the same process.
// `GetUIThreadTaskRunner()` is not known to be costly.
class TaskRunnerWithSynchronousRunOnUIThread
    : public base::SequencedTaskRunner {
 public:
  static scoped_refptr<base::SequencedTaskRunner> GetInstance() {
    static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
        instance(
            base::MakeRefCounted<TaskRunnerWithSynchronousRunOnUIThread>());
    return *instance;
  }

  TaskRunnerWithSynchronousRunOnUIThread() = default;

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    auto task_runner = GetUITaskRunner();
    if (task_runner->RunsTasksInCurrentSequence() && delay.is_zero()) {
      std::move(task).Run();
      return true;
    }
    return task_runner->PostDelayedTask(from_here, std::move(task), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    auto task_runner = GetUITaskRunner();
    if (task_runner->RunsTasksInCurrentSequence() && delay.is_zero()) {
      std::move(task).Run();
      return true;
    }
    return task_runner->PostNonNestableDelayedTask(from_here, std::move(task),
                                                   delay);
  }

  base::DelayedTaskHandle PostCancelableDelayedTask(
      base::subtle::PostDelayedTaskPassKey pass_key,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeDelta delay) override {
    // There is no call to this method on the Performance Manager `TaskRunner`.
    //
    // All callers are annotated with `base::subtle::PostDelayedTaskPassKey`. In
    // most cases, it's trivial to verify that they don't target this
    // `TaskRunner`. To confirm that no calls are made via timers defined in
    // base/timer/timer.cc, we manually verified that no `TaskRunner` obtained
    // from `PerformanceManager(Impl)::GetTaskRunner()` is passed to
    // `base::TimerBase::SetTaskRunner()`.
    NOTREACHED();
  }

  base::DelayedTaskHandle PostCancelableDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey pass_key,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeTicks delayed_run_time,
      base::subtle::DelayPolicy delay_policy) override {
    // There is no call to this method on the Performance Manager `TaskRunner`.
    //
    // See notes in `PostCancelableDelayedTask`.
    NOTREACHED();
  }

  bool PostDelayedTaskAt(base::subtle::PostDelayedTaskPassKey pass_key,
                         const base::Location& from_here,
                         base::OnceClosure task,
                         base::TimeTicks delayed_run_time,
                         base::subtle::DelayPolicy delay_policy) override {
    // There is no call to this method on the Performance Manager `TaskRunner`.
    //
    // See notes in `PostCancelableDelayedTask`.
    NOTREACHED();
  }

  bool RunOrPostTask(base::subtle::RunOrPostTaskPassKey,
                     const base::Location& from_here,
                     base::OnceClosure task) override {
    // There is no call to this method on the Performance Manager `TaskRunner`.
    // The only call is in ipc/ipc_mojo_bootstrap.cc and it's trivial to verify
    // that it doesn't target this `TaskRunner`.
    NOTREACHED();
  }

  bool RunsTasksInCurrentSequence() const override {
    return GetUITaskRunner()->RunsTasksInCurrentSequence();
  }

 private:
  ~TaskRunnerWithSynchronousRunOnUIThread() override = default;
};

}  // namespace

// static
bool PerformanceManager::IsAvailable() {
  return g_pm_is_available;
}

PerformanceManagerImpl::~PerformanceManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(g_performance_manager, this);
  // TODO(crbug.com/40629049): Move this to a TearDown function.
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
    FrameNodeImpl* outer_document_for_fenced_frame,
    int render_frame_id,
    const blink::LocalFrameToken& frame_token,
    content::BrowsingInstanceId browsing_instance_id,
    content::SiteInstanceGroupId site_instance_group_id,
    bool is_current,
    FrameNodeCreationCallback creation_callback) {
  return CreateNodeImpl<FrameNodeImpl>(
      std::move(creation_callback), process_node, page_node, parent_frame_node,
      outer_document_for_fenced_frame, render_frame_id, frame_token,
      browsing_instance_id, site_instance_group_id, is_current);
}

// static
std::unique_ptr<PageNodeImpl> PerformanceManagerImpl::CreatePageNode(
    base::WeakPtr<content::WebContents> web_contents,
    const std::string& browser_context_id,
    const GURL& visible_url,
    PagePropertyFlags initial_property_flags,
    base::TimeTicks visibility_change_time) {
  return CreateNodeImpl<PageNodeImpl>(
      base::OnceCallback<void(PageNodeImpl*)>(), std::move(web_contents),
      browser_context_id, visible_url, initial_property_flags,
      visibility_change_time);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    BrowserProcessNodeTag tag) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(), tag);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    RenderProcessHostProxy render_process_host_proxy,
    base::TaskPriority priority) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(),
      std::move(render_process_host_proxy), priority);
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
    const blink::WorkerToken& worker_token,
    const url::Origin& origin) {
  return CreateNodeImpl<WorkerNodeImpl>(
      base::OnceCallback<void(WorkerNodeImpl*)>(), browser_context_id,
      worker_type, process_node, worker_token, origin);
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
}

// static
scoped_refptr<base::SequencedTaskRunner>
PerformanceManagerImpl::GetTaskRunner() {
  if (base::FeatureList::IsEnabled(features::kRunOnMainThreadSync)) {
    return TaskRunnerWithSynchronousRunOnUIThread::GetInstance();
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
    switch (node->GetNodeType()) {
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
      case SystemNodeImpl::Type(): {
        NOTREACHED_IN_MIGRATION();
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
