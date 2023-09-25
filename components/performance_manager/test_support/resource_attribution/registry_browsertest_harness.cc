// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/resource_attribution/registry_browsertest_harness.h"

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager::resource_attribution {

RegistryBrowserTestHarness::RegistryBrowserTestHarness(bool enable_registries)
    : enable_registries_(enable_registries) {}

RegistryBrowserTestHarness::~RegistryBrowserTestHarness() = default;

ResourceContext RegistryBrowserTestHarness::GetWebContentsPageContext() const {
  // This gets the ResourceContext from the PM node so that it doesn't
  // depend on the registries which are being tested.
  ResourceContext resource_context;
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&resource_context, page_node] {
                   ASSERT_TRUE(page_node);
                   resource_context = page_node->GetResourceContext();
                 }).Then(run_loop.QuitClosure()));
  run_loop.Run();
  return resource_context;
}

void RegistryBrowserTestHarness::CreateNodes() {
  // Navigate to an initial page. This will create frames for a.com and b.com.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("a.com", "/a_embeds_b.html")));
  web_contents_loaded_page_ = true;

  // a.com is the main frame.
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_rfh);
  main_frame_id_ = main_rfh->GetGlobalId();

  // Find the embedded b.com frame.
  content::RenderFrameHost* child_rfh = nullptr;
  main_rfh->ForEachRenderFrameHostWithAction(
      [&child_rfh, main_rfh](content::RenderFrameHost* rfh) {
        if (rfh == main_rfh) {
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        }
        child_rfh = rfh;
        return content::RenderFrameHost::FrameIterationAction::kStop;
      });
  ASSERT_TRUE(child_rfh);
  sub_frame_id_ = child_rfh->GetGlobalId();
  ASSERT_NE(main_frame_id_, sub_frame_id_);

  // Wait for PerformanceManager to register the created nodes.
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

void RegistryBrowserTestHarness::DeleteNodes() {
  if (web_contents_loaded_page_) {
    // Close the page to destroy the frames.
    ASSERT_TRUE(web_contents());
    content::RenderProcessHostWatcher watcher(
        web_contents(),
        content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
    web_contents()->ClosePage();
    watcher.Wait();
    web_contents_loaded_page_ = false;
  }

  // Wait for PerformanceManager to register deleted nodes (including any
  // deleted by DeleteNodes() overrides).
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

void RegistryBrowserTestHarness::SetUp() {
  if (enable_registries_) {
    GetGraphFeatures().EnableResourceAttributionRegistries();
  }
  Super::SetUp();
}

void RegistryBrowserTestHarness::PreRunTestOnMainThread() {
  Super::PreRunTestOnMainThread();
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
}

void RegistryBrowserTestHarness::PostRunTestOnMainThread() {
  DeleteNodes();
  Super::PostRunTestOnMainThread();
}

RemoveFrameNodeWaiter::RemoveFrameNodeWaiter(
    base::WeakPtr<FrameNode> watched_node,
    Super::OnRemovedCallback on_removed_callback)
    : Super(watched_node,
            std::move(on_removed_callback),
            &Graph::AddFrameNodeObserver,
            &Graph::RemoveFrameNodeObserver) {}

void RemoveFrameNodeWaiter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  OnBeforeNodeRemoved(frame_node);
}

RemovePageNodeWaiter::RemovePageNodeWaiter(
    base::WeakPtr<PageNode> watched_node,
    Super::OnRemovedCallback on_removed_callback)
    : Super(watched_node,
            std::move(on_removed_callback),
            &Graph::AddPageNodeObserver,
            &Graph::RemovePageNodeObserver) {}

void RemovePageNodeWaiter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  OnBeforeNodeRemoved(page_node);
}

RemoveProcessNodeWaiter::RemoveProcessNodeWaiter(
    base::WeakPtr<ProcessNode> watched_node,
    Super::OnRemovedCallback on_removed_callback)
    : Super(watched_node,
            std::move(on_removed_callback),
            &Graph::AddProcessNodeObserver,
            &Graph::RemoveProcessNodeObserver) {}

void RemoveProcessNodeWaiter::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  OnBeforeNodeRemoved(process_node);
}

RemoveWorkerNodeWaiter::RemoveWorkerNodeWaiter(
    base::WeakPtr<WorkerNode> watched_node,
    Super::OnRemovedCallback on_removed_callback)
    : Super(watched_node,
            std::move(on_removed_callback),
            &Graph::AddWorkerNodeObserver,
            &Graph::RemoveWorkerNodeObserver) {}

void RemoveWorkerNodeWaiter::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  OnBeforeNodeRemoved(worker_node);
}

}  // namespace performance_manager::resource_attribution
