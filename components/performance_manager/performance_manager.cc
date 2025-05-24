// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

PerformanceManager::PerformanceManager() = default;
PerformanceManager::~PerformanceManager() = default;

// static
Graph* PerformanceManager::GetGraph() {
  return PerformanceManagerImpl::GetGraphImpl();
}

// static
base::WeakPtr<PageNode> PerformanceManager::GetPrimaryPageNodeForWebContents(
    content::WebContents* wc) {
  DCHECK(wc);
  PerformanceManagerTabHelper* helper =
      PerformanceManagerTabHelper::FromWebContents(wc);
  if (!helper)
    return nullptr;
  return helper->primary_page_node()->GetWeakPtr();
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
  return frame_node->GetWeakPtr();
}

// static
base::WeakPtr<ProcessNode>
PerformanceManager::GetProcessNodeForBrowserProcess() {
  auto* registry = PerformanceManagerRegistryImpl::GetInstance();
  if (!registry) {
    return nullptr;
  }
  ProcessNodeImpl* process_node = registry->GetBrowserProcessNode();
  return process_node ? process_node->GetWeakPtr() : nullptr;
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
  return user_data->process_node()->GetWeakPtr();
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
  return process_node ? process_node->GetWeakPtr() : nullptr;
}

// static
base::WeakPtr<WorkerNode> PerformanceManager::GetWorkerNodeForToken(
    const blink::WorkerToken& token) {
  auto* registry = PerformanceManagerRegistryImpl::GetInstance();
  if (!registry) {
    return nullptr;
  }
  WorkerNodeImpl* worker_node = registry->FindWorkerNodeForToken(token);
  return worker_node ? worker_node->GetWeakPtr() : nullptr;
}

// static
void PerformanceManager::AddObserver(PerformanceManagerObserver* observer) {
  PerformanceManagerRegistryImpl::GetInstance()->AddObserver(observer);
}

// static
void PerformanceManager::RemoveObserver(PerformanceManagerObserver* observer) {
  PerformanceManagerRegistryImpl::GetInstance()->RemoveObserver(observer);
}

// static
void PerformanceManager::RecordMemoryMetrics() {
  if (IsAvailable()) {
    resource_attribution::internal::QueryScheduler::GetFromGraph()
        ->RecordMemoryMetrics();
  }
}

}  // namespace performance_manager
