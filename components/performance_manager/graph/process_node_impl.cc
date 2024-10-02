// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/process_node_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/named_trigger.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {

// CHECK's that `process_type` is appropriate for a BrowserChildProcessHost and
// returns it. This is called from a ProcessNodeImpl initializer so that the
// type is checked before the constructor body.
content::ProcessType ValidateBrowserChildProcessType(
    content::ProcessType process_type) {
  CHECK_NE(process_type, content::PROCESS_TYPE_BROWSER);
  CHECK_NE(process_type, content::PROCESS_TYPE_RENDERER);
  return process_type;
}

}  // namespace

ProcessNodeImpl::ProcessNodeImpl(BrowserProcessNodeTag tag)
    : ProcessNodeImpl(content::PROCESS_TYPE_BROWSER,
                      AnyChildProcessHostProxy{},
                      base::TaskPriority::HIGHEST) {}

ProcessNodeImpl::ProcessNodeImpl(RenderProcessHostProxy proxy,
                                 base::TaskPriority priority)
    : ProcessNodeImpl(content::PROCESS_TYPE_RENDERER,
                      AnyChildProcessHostProxy(std::move(proxy)),
                      priority) {}

ProcessNodeImpl::ProcessNodeImpl(content::ProcessType process_type,
                                 BrowserChildProcessHostProxy proxy)
    : ProcessNodeImpl(ValidateBrowserChildProcessType(process_type),
                      AnyChildProcessHostProxy(std::move(proxy)),
                      base::TaskPriority::HIGHEST) {}

ProcessNodeImpl::ProcessNodeImpl(content::ProcessType process_type,
                                 AnyChildProcessHostProxy proxy,
                                 base::TaskPriority priority)
    : process_type_(process_type),
      child_process_host_proxy_(std::move(proxy)),
      priority_(priority) {
  // Nodes are created on the UI thread, then accessed on the PM sequence.
  // `weak_this_` can be returned from GetWeakPtrOnUIThread() and dereferenced
  // on the PM sequence.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  weak_this_ = weak_factory_.GetWeakPtr();

  // Child process nodes must have a valid proxy.
  switch (process_type) {
    case content::PROCESS_TYPE_BROWSER:
      // Do nothing.
      break;
    case content::PROCESS_TYPE_RENDERER:
      CHECK(absl::get<RenderProcessHostProxy>(child_process_host_proxy_)
                .is_valid());
      break;
    default:
      CHECK(absl::get<BrowserChildProcessHostProxy>(child_process_host_proxy_)
                .is_valid());
      break;
  }
}

ProcessNodeImpl::~ProcessNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Crash if this process node is destroyed while still hosting a worker node.
  // TODO(crbug.com/40051698): Turn this into a DCHECK once the issue is
  //                                  resolved.
  CHECK(worker_nodes_.empty());
}

void ProcessNodeImpl::Bind(
    mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // A RenderProcessHost can be reused if the backing process suddenly dies, in
  // which case we will receive a new receiver from the newly spawned process.
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ProcessNodeImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);

  main_thread_task_load_is_low_.SetAndMaybeNotify(this,
                                                  main_thread_task_load_is_low);
}

void ProcessNodeImpl::OnV8ContextCreated(
    mojom::V8ContextDescriptionPtr description,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph())) {
    tracker->OnV8ContextCreated(PassKey(), this, *description,
                                std::move(iframe_attribution_data));
  }
}

void ProcessNodeImpl::OnV8ContextDetached(
    const blink::V8ContextToken& v8_context_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph()))
    tracker->OnV8ContextDetached(PassKey(), this, v8_context_token);
}

void ProcessNodeImpl::OnV8ContextDestroyed(
    const blink::V8ContextToken& v8_context_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph()))
    tracker->OnV8ContextDestroyed(PassKey(), this, v8_context_token);
}

void ProcessNodeImpl::OnRemoteIframeAttached(
    const blink::LocalFrameToken& parent_frame_token,
    const blink::RemoteFrameToken& remote_frame_token,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph())) {
    auto* ec_registry =
        execution_context::ExecutionContextRegistry::GetFromGraph(graph());
    DCHECK(ec_registry);
    auto* parent_frame_node =
        ec_registry->GetFrameNodeByFrameToken(parent_frame_token);
    if (parent_frame_node) {
      tracker->OnRemoteIframeAttached(
          PassKey(), FrameNodeImpl::FromNode(parent_frame_node),
          remote_frame_token, std::move(iframe_attribution_data));
    }
  }
}

void ProcessNodeImpl::OnRemoteIframeDetached(
    const blink::LocalFrameToken& parent_frame_token,
    const blink::RemoteFrameToken& remote_frame_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph())) {
    auto* ec_registry =
        execution_context::ExecutionContextRegistry::GetFromGraph(graph());
    DCHECK(ec_registry);
    auto* parent_frame_node =
        ec_registry->GetFrameNodeByFrameToken(parent_frame_token);
    if (parent_frame_node) {
      tracker->OnRemoteIframeDetached(
          PassKey(), FrameNodeImpl::FromNode(parent_frame_node),
          remote_frame_token);
    }
  }
}

content::ProcessType ProcessNodeImpl::GetProcessType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_type_;
}

base::ProcessId ProcessNodeImpl::GetProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_id_;
}

const base::Process& ProcessNodeImpl::GetProcess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_.value();
}

resource_attribution::ProcessContext ProcessNodeImpl::GetResourceContext()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resource_attribution::ProcessContext::FromProcessNode(this);
}

base::TimeTicks ProcessNodeImpl::GetLaunchTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return launch_time_;
}

std::optional<int32_t> ProcessNodeImpl::GetExitStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return exit_status_;
}

const std::string& ProcessNodeImpl::GetMetricsName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metrics_name_;
}

bool ProcessNodeImpl::GetMainThreadTaskLoadIsLow() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  return main_thread_task_load_is_low_.value();
}

uint64_t ProcessNodeImpl::GetPrivateFootprintKb() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_footprint_kb_;
}

uint64_t ProcessNodeImpl::GetResidentSetKb() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resident_set_kb_;
}

uint64_t ProcessNodeImpl::GetPrivateSwapKb() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_swap_kb_;
}

RenderProcessHostId ProcessNodeImpl::GetRenderProcessHostId() const {
  return GetRenderProcessHostProxy().render_process_host_id();
}

const RenderProcessHostProxy& ProcessNodeImpl::GetRenderProcessHostProxy()
    const {
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  return absl::get<RenderProcessHostProxy>(child_process_host_proxy_);
}

const BrowserChildProcessHostProxy&
ProcessNodeImpl::GetBrowserChildProcessHostProxy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(process_type_, content::PROCESS_TYPE_BROWSER);
  DCHECK_NE(process_type_, content::PROCESS_TYPE_RENDERER);
  return absl::get<BrowserChildProcessHostProxy>(child_process_host_proxy_);
}

base::TaskPriority ProcessNodeImpl::GetPriority() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return priority_.value();
}

ProcessNode::ContentTypes ProcessNodeImpl::GetHostedContentTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  return hosted_content_types_;
}

ProcessNode::NodeSetView<FrameNodeImpl*> ProcessNodeImpl::frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  return NodeSetView<FrameNodeImpl*>(frame_nodes_);
}

ProcessNode::NodeSetView<WorkerNodeImpl*> ProcessNodeImpl::worker_nodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  return NodeSetView<WorkerNodeImpl*>(worker_nodes_);
}

void ProcessNodeImpl::SetProcessExitStatus(int32_t exit_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This may occur as the first event seen in the case where the process
  // fails to start or suffers a startup crash.
  exit_status_ = exit_status;

  // Close the process handle to kill the zombie.
  process_.SetAndNotify(this, base::Process());

  // No more message should be received from this process.
  receiver_.reset();
}

void ProcessNodeImpl::SetProcessMetricsName(const std::string& metrics_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_name_ = metrics_name;
}

void ProcessNodeImpl::SetProcess(base::Process process,
                                 base::TimeTicks launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process.IsValid());
  // Either this is the initial process associated with this process node,
  // or it's a subsequent process. In the latter case, there must have been
  // an exit status associated with the previous process.
  DCHECK(!process_.value().IsValid() || exit_status_.has_value());

  base::ProcessId pid = process.Pid();
  SetProcessImpl(std::move(process), pid, launch_time);
}

void ProcessNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  const bool inserted = frame_nodes_.insert(frame_node).second;
  DCHECK(inserted);
}

void ProcessNodeImpl::RemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  DCHECK(base::Contains(frame_nodes_, frame_node));
  frame_nodes_.erase(frame_node);
}

void ProcessNodeImpl::AddWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  const bool inserted = worker_nodes_.insert(worker_node).second;
  DCHECK(inserted);
}

void ProcessNodeImpl::RemoveWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  DCHECK(base::Contains(worker_nodes_, worker_node));
  worker_nodes_.erase(worker_node);
}

void ProcessNodeImpl::set_priority(base::TaskPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  priority_.SetAndMaybeNotify(this, priority);
}

void ProcessNodeImpl::add_hosted_content_type(ContentType content_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  hosted_content_types_.Put(content_type);
}

base::WeakPtr<ProcessNodeImpl> ProcessNodeImpl::GetWeakPtrOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_this_;
}

base::WeakPtr<ProcessNodeImpl> ProcessNodeImpl::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ProcessNodeImpl::SetProcessImpl(base::Process process,
                                     base::ProcessId new_pid,
                                     base::TimeTicks launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process.IsValid());

  graph()->BeforeProcessPidChange(this, new_pid);

  // Clear the exit status for the previous process (if any).
  exit_status_.reset();

  // Also clear the measurement data (if any), as it references the previous
  // process.
  private_footprint_kb_ = 0;
  resident_set_kb_ = 0;
  private_swap_kb_ = 0;

  process_id_ = new_pid;
  launch_time_ = launch_time;

  // Set the process variable last, as it will fire the notification.
  process_.SetAndNotify(this, std::move(process));
}

ProcessNode::NodeSetView<const FrameNode*> ProcessNodeImpl::GetFrameNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const FrameNode*>(frame_nodes_);
}

ProcessNode::NodeSetView<const WorkerNode*> ProcessNodeImpl::GetWorkerNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const WorkerNode*>(worker_nodes_);
}

void ProcessNodeImpl::OnAllFramesInProcessFrozen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(process_type_, content::PROCESS_TYPE_RENDERER);
  for (auto& observer : GetObservers()) {
    observer.OnAllFramesInProcessFrozen(this);
  }
}

void ProcessNodeImpl::OnJoiningGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure all weak pointers, even `weak_this_` that was created on the UI
  // thread in the constructor, can only be dereferenced on the graph sequence.
  weak_factory_.BindToCurrentSequence(
      base::subtle::BindWeakPtrFactoryPassKey());

  NodeAttachedDataStorage::Create(this);
}

void ProcessNodeImpl::OnBeforeLeavingGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make as if we're transitioning to the null PID before we die to clear this
  // instance from the PID map.
  if (process_id_ != base::kNullProcessId)
    graph()->BeforeProcessPidChange(this, base::kNullProcessId);

  // All child frames should have been removed before the process is removed.
  DCHECK(frame_nodes_.empty());
}

void ProcessNodeImpl::RemoveNodeAttachedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyNodeInlineDataStorage();
}

}  // namespace performance_manager
