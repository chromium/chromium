// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/worker_context.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "components/performance_manager/worker_watcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace resource_attribution {

namespace {

using ResourceAttrWorkerContextTest =
    performance_manager::PerformanceManagerTestHarness;
using ResourceAttrWorkerContextNoPMTest = content::RenderViewHostTestHarness;

TEST_F(ResourceAttrWorkerContextTest, WorkerContexts) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  performance_manager::WorkerWatcher* worker_watcher =
      performance_manager::PerformanceManagerRegistryImpl::GetInstance()
          ->GetWorkerWatcherForTesting(GetBrowserContext());
  ASSERT_TRUE(worker_watcher);

  // Navigate to an initial page. This creates a TestFrameNode and
  // TestProcessNode.
  content::RenderFrameHost* rfh =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents.get(), GURL("https://a.com/"));
  ASSERT_TRUE(rfh);

  // Simulate two workers.
  blink::DedicatedWorkerToken worker_token;
  blink::DedicatedWorkerToken worker_token2;
  ASSERT_NE(worker_token, worker_token2);

  worker_watcher->OnWorkerCreated(worker_token, rfh->GetProcess()->GetID(),
                                  rfh->GetLastCommittedOrigin(),
                                  rfh->GetGlobalId());
  worker_watcher->OnWorkerCreated(worker_token2, rfh->GetProcess()->GetID(),
                                  rfh->GetLastCommittedOrigin(),
                                  rfh->GetGlobalId());
  absl::Cleanup delete_workers = [&] {
    worker_watcher->OnBeforeWorkerDestroyed(worker_token, rfh->GetGlobalId());
    worker_watcher->OnBeforeWorkerDestroyed(worker_token2, rfh->GetGlobalId());
  };

  // Validate that the right worker nodes were created, save a pointer to one.
  base::WeakPtr<WorkerNode> worker_node;
  performance_manager::RunInGraph([&](Graph* graph) {
    bool found_worker = false;
    for (const WorkerNode* node : graph->GetAllWorkerNodes()) {
      EXPECT_THAT(node->GetWorkerToken(),
                  ::testing::AnyOf(worker_token, worker_token2));
      if (node->GetWorkerToken() == worker_token) {
        worker_node = WorkerNodeImpl::FromNode(node)->GetWeakPtr();
        found_worker = true;
      }
    }
    EXPECT_TRUE(found_worker);
  });

  std::optional<WorkerContext> worker_context =
      WorkerContext::FromWorkerToken(worker_token);
  ASSERT_TRUE(worker_context.has_value());
  EXPECT_EQ(worker_token, worker_context->GetWorkerToken());

  base::WeakPtr<WorkerNode> worker_node_from_context =
      worker_context->GetWeakWorkerNode();
  performance_manager::RunInGraph([&] {
    ASSERT_TRUE(worker_node);
    ASSERT_TRUE(worker_node_from_context);
    EXPECT_EQ(worker_node.get(), worker_node_from_context.get());

    EXPECT_EQ(worker_node.get(), worker_context->GetWorkerNode());
    EXPECT_EQ(worker_context.value(), worker_node->GetResourceContext());
    EXPECT_EQ(worker_context.value(),
              WorkerContext::FromWorkerNode(worker_node.get()));
    EXPECT_EQ(worker_context.value(),
              WorkerContext::FromWeakWorkerNode(worker_node));
  });

  // Make sure a second worker gets a different context.
  std::optional<WorkerContext> worker_context2 =
      WorkerContext::FromWorkerToken(worker_token2);
  EXPECT_TRUE(worker_context2.has_value());
  EXPECT_NE(worker_context2, worker_context);

  std::move(delete_workers).Invoke();

  // Context still returns worker token, but it no longer matches any worker.
  EXPECT_EQ(worker_token, worker_context->GetWorkerToken());
  EXPECT_FALSE(WorkerContext::FromWorkerToken(worker_token).has_value());
  performance_manager::RunInGraph([&](Graph* graph) {
    EXPECT_EQ(graph->GetAllWorkerNodes().size(), 0u);
    EXPECT_FALSE(worker_node);
    EXPECT_EQ(nullptr, worker_context->GetWorkerNode());
    EXPECT_EQ(std::nullopt, WorkerContext::FromWeakWorkerNode(worker_node));
  });
}

TEST_F(ResourceAttrWorkerContextNoPMTest, WorkerContextWithoutPM) {
  // Unknown worker token should not return a context. This also covers the
  // case where a worker exists for the token but PerformanceManager isn't
  // initialized so doesn't have a WorkerNode for it.
  EXPECT_FALSE(WorkerContext::FromWorkerToken(blink::DedicatedWorkerToken())
                   .has_value());
}

}  // namespace

}  // namespace resource_attribution
