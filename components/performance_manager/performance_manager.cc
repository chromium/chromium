// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/performance_manager_owned.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

PerformanceManager::PerformanceManager() = default;
PerformanceManager::~PerformanceManager() = default;

// static
void PerformanceManager::CallOnGraph(const base::Location& from_here,
                                     base::OnceClosure callback) {
  DCHECK(callback);

  PerformanceManagerImpl::GetTaskRunner()->PostTask(from_here,
                                                    std::move(callback));
}
// static
void PerformanceManager::CallOnGraph(const base::Location& from_here,
                                     GraphCallback callback) {
  DCHECK(callback);

  // TODO(siggi): Unwrap this by binding the loose param.
  PerformanceManagerImpl::GetTaskRunner()->PostTask(
      from_here, base::BindOnce(&PerformanceManagerImpl::RunCallbackWithGraph,
                                std::move(callback)));
}

// static
void PerformanceManager::PassToGraph(const base::Location& from_here,
                                     std::unique_ptr<GraphOwned> graph_owned) {
  DCHECK(graph_owned);

  // PassToGraph() should only be called when a graph is available to take
  // ownership of |graph_owned|.
  DCHECK(IsAvailable());

  PerformanceManagerImpl::CallOnGraphImpl(
      from_here,
      base::BindOnce(
          [](std::unique_ptr<GraphOwned> graph_owned, GraphImpl* graph) {
            graph->PassToGraph(std::move(graph_owned));
          },
          std::move(graph_owned)));
}

// static
base::WeakPtr<PageNode> PerformanceManager::GetPrimaryPageNodeForWebContents(
    content::WebContents* wc) {
  DCHECK(wc);
  PerformanceManagerTabHelper* helper =
      PerformanceManagerTabHelper::FromWebContents(wc);
  if (!helper)
    return nullptr;
  return helper->primary_page_node()->GetWeakPtrOnUIThread();
}

// static
base::WeakPtr<FrameNode> PerformanceManager::GetFrameNodeForRenderFrameHost(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  auto* wc = content::WebContents::FromRenderFrameHost(rfh);
  PerformanceManagerTabHelper* helper =
      PerformanceManagerTabHelper::FromWebContents(wc);
  if (!helper)
    return nullptr;
  auto* frame_node = helper->GetFrameNode(rfh);
  if (!frame_node) {
    // This should only happen if GetFrameNodeForRenderFrameHost is called
    // before the RenderFrameCreated notification is dispatched.
    DCHECK(!rfh->IsRenderFrameLive());
    return nullptr;
  }
  return frame_node->GetWeakPtrOnUIThread();
}

// static
base::WeakPtr<ProcessNode>
PerformanceManager::GetProcessNodeForBrowserProcess() {
  auto* registry = PerformanceManagerRegistryImpl::GetInstance();
  if (!registry) {
    return nullptr;
  }
  ProcessNodeImpl* process_node = registry->GetBrowserProcessNode();
  return process_node ? process_node->GetWeakPtrOnUIThread() : nullptr;
}

// static
base::WeakPtr<ProcessNode>
PerformanceManager::GetProcessNodeForRenderProcessHost(
    content::RenderProcessHost* rph) {
  DCHECK(rph);
  auto* user_data = RenderProcessUserData::GetForRenderProcessHost(rph);
  // There is a window after a RenderProcessHost is created before
  // PerformanceManagerRegistry::CreateProcessNode() is called, during which
  // time the RenderProcessUserData is not attached to the RPH yet. (It's called
  // indirectly from RenderProcessHost::Init.)
  if (!user_data)
    return nullptr;
  return user_data->process_node()->GetWeakPtrOnUIThread();
}

// static
base::WeakPtr<ProcessNode>
PerformanceManager::GetProcessNodeForRenderProcessHostId(
    RenderProcessHostId id) {
  DCHECK(id);
  auto* rph = content::RenderProcessHost::FromID(id.value());
  if (!rph)
    return nullptr;
  return GetProcessNodeForRenderProcessHost(rph);
}

// static
base::WeakPtr<ProcessNode>
PerformanceManager::GetProcessNodeForBrowserChildProcessHost(
    content::BrowserChildProcessHost* bcph) {
  DCHECK(bcph);
  return GetProcessNodeForBrowserChildProcessHostId(
      BrowserChildProcessHostId(bcph->GetData().id));
}

// static
base::WeakPtr<ProcessNode>
PerformanceManager::GetProcessNodeForBrowserChildProcessHostId(
    BrowserChildProcessHostId id) {
  DCHECK(id);
  auto* registry = PerformanceManagerRegistryImpl::GetInstance();
  if (!registry) {
    return nullptr;
  }
  ProcessNodeImpl* process_node = registry->GetBrowserChildProcessNode(id);
  return process_node ? process_node->GetWeakPtrOnUIThread() : nullptr;
}

// static
base::WeakPtr<WorkerNode> PerformanceManager::GetWorkerNodeForToken(
    const blink::WorkerToken& token) {
  auto* registry = PerformanceManagerRegistryImpl::GetInstance();
  if (!registry) {
    return nullptr;
  }
  WorkerNodeImpl* worker_node = registry->FindWorkerNodeForToken(token);
  return worker_node ? worker_node->GetWeakPtrOnUIThread() : nullptr;
}

// static
void PerformanceManager::AddObserver(
    PerformanceManagerMainThreadObserver* observer) {
  PerformanceManagerRegistryImpl::GetInstance()->AddObserver(observer);
}

// static
void PerformanceManager::RemoveObserver(
    PerformanceManagerMainThreadObserver* observer) {
  PerformanceManagerRegistryImpl::GetInstance()->RemoveObserver(observer);
}

// static
void PerformanceManager::AddMechanism(
    PerformanceManagerMainThreadMechanism* mechanism) {
  PerformanceManagerRegistryImpl::GetInstance()->AddMechanism(mechanism);
}

// static
void PerformanceManager::RemoveMechanism(
    PerformanceManagerMainThreadMechanism* mechanism) {
  PerformanceManagerRegistryImpl::GetInstance()->RemoveMechanism(mechanism);
}

// static
bool PerformanceManager::HasMechanism(
    PerformanceManagerMainThreadMechanism* mechanism) {
  return PerformanceManagerRegistryImpl::GetInstance()->HasMechanism(mechanism);
}

// static
void PerformanceManager::PassToPM(
    std::unique_ptr<PerformanceManagerOwned> pm_owned) {
  return PerformanceManagerRegistryImpl::GetInstance()->PassToPM(
      std::move(pm_owned));
}

// static
std::unique_ptr<PerformanceManagerOwned> PerformanceManager::TakeFromPM(
    PerformanceManagerOwned* pm_owned) {
  return PerformanceManagerRegistryImpl::GetInstance()->TakeFromPM(pm_owned);
}

// static
void PerformanceManager::RegisterObject(
    PerformanceManagerRegistered* pm_object) {
  return PerformanceManagerRegistryImpl::GetInstance()->RegisterObject(
      pm_object);
}

// static
void PerformanceManager::UnregisterObject(
    PerformanceManagerRegistered* pm_object) {
  return PerformanceManagerRegistryImpl::GetInstance()->UnregisterObject(
      pm_object);
}

// static
PerformanceManagerRegistered* PerformanceManager::GetRegisteredObject(
    uintptr_t type_id) {
  return PerformanceManagerRegistryImpl::GetInstance()->GetRegisteredObject(
      type_id);
}

// static
scoped_refptr<base::SequencedTaskRunner> PerformanceManager::GetTaskRunner() {
  return PerformanceManagerImpl::GetTaskRunner();
}

// static
void PerformanceManager::RecordMemoryMetrics() {
  using QueryScheduler = resource_attribution::internal::QueryScheduler;
  QueryScheduler::CallWithScheduler(
      base::BindOnce(&QueryScheduler::RecordMemoryMetrics));
}

}  // namespace performance_manager
