// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/worker_context_registry.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/resource_attribution/registry_browsertest_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace performance_manager::resource_attribution {

namespace {

// A test harness that can create WorkerNodes to test with the
// WorkerContextRegistry.
class WorkerContextRegistryTest : public RegistryBrowserTestHarness {
 public:
  using Super = RegistryBrowserTestHarness;

  explicit WorkerContextRegistryTest(bool enable_registries = true)
      : Super(enable_registries) {}

  void SetUpOnMainThread() override {
    Super::SetUpOnMainThread();

    // Workers require HTTPS. Replace the default HTTP server.
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(https_server_.Start());

    // Enable WorkerWatcher for the WebContents. In production this is done in
    // ChromeBrowserMainExtraPartsPerformanceManager.
    tracked_browser_context_ = web_contents()->GetBrowserContext();
    PerformanceManagerRegistry::GetInstance()->NotifyBrowserContextAdded(
        tracked_browser_context_);
  }

  void TearDownOnMainThread() override {
    PerformanceManagerRegistry::GetInstance()->NotifyBrowserContextRemoved(
        tracked_browser_context_);
    tracked_browser_context_ = nullptr;
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    Super::TearDownOnMainThread();
  }

  void CreateNodes() override {
    // Don't load the normal frames in Super::CreateNodes(). Instead, navigate
    // to a page that loads some workers, and wait until they're registered.
    content::TitleWatcher title_watcher(web_contents(), u"OK");
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        https_server_.GetURL("a.test", "/workers/multi_worker.html")));
    ASSERT_EQ(u"OK", title_watcher.WaitAndGetTitle());

    // Make DeleteNodes() close the page to destroy the workers.
    web_contents_loaded_page_ = true;

    // Save details about two arbitrary workers.
    base::RunLoop run_loop;
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE, base::BindLambdaForTesting([&](GraphImpl* graph) {
                     const std::vector<WorkerNodeImpl*> worker_nodes =
                         graph->GetAllWorkerNodeImpls();
                     ASSERT_GE(worker_nodes.size(), 2u);
                     worker_token_a_ = worker_nodes[0]->worker_token();
                     worker_token_b_ = worker_nodes[1]->worker_token();
                     weak_worker_node_a_ = worker_nodes[0]->GetWeakPtr();
                     weak_worker_node_b_ = worker_nodes[1]->GetWeakPtr();
                   }).Then(run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_NE(worker_token_a_, worker_token_b_);
  }

 protected:
  // Details of the workers created by CreateNodes().
  blink::WorkerToken worker_token_a_;
  blink::WorkerToken worker_token_b_;
  base::WeakPtr<WorkerNode> weak_worker_node_a_;
  base::WeakPtr<WorkerNode> weak_worker_node_b_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  // A BrowserContext that PerformanceManagerRegistry is tracking.
  raw_ptr<content::BrowserContext> tracked_browser_context_;
};

class WorkerContextRegistryDisabledTest : public WorkerContextRegistryTest {
 public:
  WorkerContextRegistryDisabledTest() : WorkerContextRegistryTest(false) {}
};

IN_PROC_BROWSER_TEST_F(WorkerContextRegistryTest, WorkerContexts) {
  CreateNodes();

  absl::optional<WorkerContext> context_from_worker_token =
      WorkerContextRegistry::ContextForWorkerToken(worker_token_a_);
  ASSERT_TRUE(context_from_worker_token.has_value());
  const WorkerContext worker_context = context_from_worker_token.value();
  const ResourceContext resource_context = worker_context;
  EXPECT_EQ(worker_token_a_,
            WorkerContextRegistry::WorkerTokenFromContext(worker_context));
  EXPECT_EQ(worker_token_a_,
            WorkerContextRegistry::WorkerTokenFromContext(resource_context));

  RunInGraphWithRegistry<WorkerContextRegistry>(
      [&](const WorkerContextRegistry* registry) {
        ASSERT_TRUE(weak_worker_node_a_);
        EXPECT_EQ(worker_context, weak_worker_node_a_->GetResourceContext());
        EXPECT_EQ(weak_worker_node_a_.get(),
                  registry->GetWorkerNodeForContext(worker_context));
        EXPECT_EQ(weak_worker_node_a_.get(),
                  registry->GetWorkerNodeForContext(resource_context));
      });

  // Make sure the second worker gets a different context token.
  absl::optional<WorkerContext> context_from_worker_token2 =
      WorkerContextRegistry::ContextForWorkerToken(worker_token_b_);
  EXPECT_TRUE(context_from_worker_token2.has_value());
  EXPECT_NE(context_from_worker_token2, context_from_worker_token);

  DeleteNodes();

  EXPECT_EQ(absl::nullopt,
            WorkerContextRegistry::ContextForWorkerToken(worker_token_a_));
  EXPECT_EQ(absl::nullopt,
            WorkerContextRegistry::WorkerTokenFromContext(worker_context));
  RunInGraphWithRegistry<WorkerContextRegistry>(
      [&](const WorkerContextRegistry* registry) {
        EXPECT_FALSE(weak_worker_node_a_);
        EXPECT_EQ(nullptr, registry->GetWorkerNodeForContext(worker_context));
        EXPECT_EQ(nullptr, registry->GetWorkerNodeForContext(resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(WorkerContextRegistryTest, InvalidWorkerContexts) {
  const auto kInvalidToken = blink::WorkerToken();

  EXPECT_EQ(absl::nullopt,
            WorkerContextRegistry::ContextForWorkerToken(kInvalidToken));

  // Find a non-WorkerNode ResourceContext.
  const ResourceContext invalid_resource_context = GetWebContentsPageContext();
  EXPECT_EQ(absl::nullopt, WorkerContextRegistry::WorkerTokenFromContext(
                               invalid_resource_context));
  RunInGraphWithRegistry<WorkerContextRegistry>(
      [&](const WorkerContextRegistry* registry) {
        EXPECT_EQ(nullptr,
                  registry->GetWorkerNodeForContext(invalid_resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(WorkerContextRegistryTest, OnBeforeWorkerNodeRemoved) {
  CreateNodes();

  absl::optional<WorkerContext> worker_context =
      WorkerContextRegistry::ContextForWorkerToken(worker_token_a_);
  ASSERT_TRUE(worker_context.has_value());

  RemoveWorkerNodeWaiter waiter(
      weak_worker_node_a_,
      base::BindLambdaForTesting([&](const WorkerNode* worker_node) {
        // `worker_node` should still be available from WorkerContextRegistry in
        // OnBeforeWorkerNodeRemoved.
        const auto* registry =
            WorkerContextRegistry::GetFromGraph(worker_node->GetGraph());
        ASSERT_TRUE(registry);
        EXPECT_EQ(registry->GetWorkerNodeForContext(worker_context.value()),
                  worker_node);
      }));

  DeleteNodes();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(WorkerContextRegistryDisabledTest, UIThreadAccess) {
  CreateNodes();

  // Static accessors should safely return null if WorkerContextRegistry is not
  // enabled in Performance Manager.
  EXPECT_EQ(absl::nullopt,
            WorkerContextRegistry::ContextForWorkerToken(worker_token_a_));

  const auto kDummyWorkerContext = WorkerContext();
  const ResourceContext kDummyResourceContext = kDummyWorkerContext;

  EXPECT_EQ(absl::nullopt,
            WorkerContextRegistry::WorkerTokenFromContext(kDummyWorkerContext));
  EXPECT_EQ(absl::nullopt, WorkerContextRegistry::WorkerTokenFromContext(
                               kDummyResourceContext));
}

}  // namespace

}  // namespace performance_manager::resource_attribution
