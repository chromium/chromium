// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/process_context_registry.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/resource_attribution/registry_browsertest_harness.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::resource_attribution {

namespace {

// A wrapper that owns a BrowserChildProcessHost and acts as a no-op
// BrowserChildProcessHostDelegate.
class TestBrowserChildProcess final
    : public content::BrowserChildProcessHostDelegate {
 public:
  explicit TestBrowserChildProcess(content::ProcessType process_type)
      : host_(content::BrowserChildProcessHost::Create(
            process_type,
            this,
            content::ChildProcessHost::IpcMode::kNormal)) {}

  content::BrowserChildProcessHost* host() const { return host_.get(); }

  BrowserChildProcessHostId id() const {
    return BrowserChildProcessHostId(host_->GetData().id);
  }

  BrowserChildProcessHostProxy proxy() const {
    return BrowserChildProcessHostProxy::CreateForTesting(id());
  }

 private:
  std::unique_ptr<content::BrowserChildProcessHost> host_;
};

class ProcessContextRegistryTest : public RegistryBrowserTestHarness {
 public:
  using Super = RegistryBrowserTestHarness;

  explicit ProcessContextRegistryTest(bool enable_registries = true)
      : Super(enable_registries) {}

  void CreateNodes() override {
    // Create PM nodes for the browser process and a non-browser child
    // process. In production non-renderer process nodes are created by
    // chrome/browser/performance_manager, which isn't hooked up in content/
    // browsertests.
    std::unique_ptr<ProcessNodeImpl> browser_process_node =
        PerformanceManagerImpl::CreateProcessNode(BrowserProcessNodeTag{});
    weak_browser_process_node_ = browser_process_node->GetWeakPtrOnUIThread();
    tracked_nodes_.push_back(std::move(browser_process_node));

    utility_process_ = std::make_unique<TestBrowserChildProcess>(
        content::ProcessType::PROCESS_TYPE_UTILITY);
    std::unique_ptr<ProcessNodeImpl> utility_process_node =
        PerformanceManagerImpl::CreateProcessNode(
            content::ProcessType::PROCESS_TYPE_UTILITY,
            utility_process_->proxy());
    weak_utility_process_node_ = utility_process_node->GetWeakPtrOnUIThread();
    tracked_nodes_.push_back(std::move(utility_process_node));

    // Navigate the WebContents to create renderer processes.
    Super::CreateNodes();

    // a.com is the main frame.
    content::RenderProcessHost* main_rph =
        content::RenderFrameHost::FromID(main_frame_id_)->GetProcess();
    ASSERT_TRUE(main_rph);
    render_process_id_a_ = RenderProcessHostId(main_rph->GetID());

    // b.com is the child frame, which should get its own process.
    content::RenderProcessHost* child_rph =
        content::RenderFrameHost::FromID(sub_frame_id_)->GetProcess();
    ASSERT_TRUE(child_rph);
    render_process_id_b_ = RenderProcessHostId(child_rph->GetID());
    ASSERT_NE(render_process_id_a_, render_process_id_b_);
  }

  void DeleteNodes() override {
    utility_process_.reset();
    PerformanceManagerImpl::BatchDeleteNodes(std::move(tracked_nodes_));
    Super::DeleteNodes();
  }

 protected:
  // Details of the child processes created by CreateNodes().
  RenderProcessHostId render_process_id_a_;
  RenderProcessHostId render_process_id_b_;
  std::unique_ptr<TestBrowserChildProcess> utility_process_;
  base::WeakPtr<ProcessNode> weak_browser_process_node_;
  base::WeakPtr<ProcessNode> weak_utility_process_node_;

  // PM nodes created in CreateNodes() that must be deleted manually.
  std::vector<std::unique_ptr<NodeBase>> tracked_nodes_;
};

class ProcessContextRegistryDisabledTest : public ProcessContextRegistryTest {
 public:
  ProcessContextRegistryDisabledTest() : ProcessContextRegistryTest(false) {}
};

IN_PROC_BROWSER_TEST_F(ProcessContextRegistryTest, BrowserProcessContext) {
  CreateNodes();

  ASSERT_TRUE(ProcessContextRegistry::BrowserProcessContext().has_value());
  const ProcessContext browser_context =
      ProcessContextRegistry::BrowserProcessContext().value();
  const ResourceContext resource_context = browser_context;
  EXPECT_TRUE(ProcessContextRegistry::IsBrowserProcessContext(browser_context));
  EXPECT_TRUE(
      ProcessContextRegistry::IsBrowserProcessContext(resource_context));

  EXPECT_FALSE(
      ProcessContextRegistry::IsRenderProcessContext(resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::RenderProcessHostFromContext(
                         resource_context));
  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserChildProcessContext(resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::BrowserChildProcessHostFromContext(
                         resource_context));

  RunInGraphWithRegistry<ProcessContextRegistry>(
      [&](const ProcessContextRegistry* registry) {
        ASSERT_TRUE(weak_browser_process_node_);
        EXPECT_EQ(browser_context,
                  weak_browser_process_node_->GetResourceContext());
        EXPECT_EQ(weak_browser_process_node_.get(),
                  registry->GetProcessNodeForContext(browser_context));
        EXPECT_EQ(weak_browser_process_node_.get(),
                  registry->GetProcessNodeForContext(resource_context));
      });

  DeleteNodes();

  EXPECT_EQ(absl::nullopt, ProcessContextRegistry::BrowserProcessContext());
  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserProcessContext(browser_context));
  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserProcessContext(resource_context));
  RunInGraphWithRegistry<ProcessContextRegistry>(
      [&](const ProcessContextRegistry* registry) {
        EXPECT_FALSE(weak_browser_process_node_);
        EXPECT_EQ(nullptr, registry->GetProcessNodeForContext(browser_context));
        EXPECT_EQ(nullptr,
                  registry->GetProcessNodeForContext(resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(ProcessContextRegistryTest, RenderProcessContext) {
  CreateNodes();

  auto* rph =
      content::RenderProcessHost::FromID(render_process_id_a_.GetUnsafeValue());
  ASSERT_TRUE(rph);
  absl::optional<ProcessContext> context_from_rph =
      ProcessContextRegistry::ContextForRenderProcessHost(rph);
  EXPECT_EQ(context_from_rph,
            ProcessContextRegistry::ContextForRenderProcessHostId(
                render_process_id_a_));

  ASSERT_TRUE(context_from_rph.has_value());
  const ProcessContext render_process_context = context_from_rph.value();
  const ResourceContext resource_context = render_process_context;
  EXPECT_TRUE(
      ProcessContextRegistry::IsRenderProcessContext(render_process_context));
  EXPECT_TRUE(ProcessContextRegistry::IsRenderProcessContext(resource_context));
  EXPECT_EQ(rph, ProcessContextRegistry::RenderProcessHostFromContext(
                     render_process_context));
  EXPECT_EQ(rph, ProcessContextRegistry::RenderProcessHostFromContext(
                     resource_context));

  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserProcessContext(resource_context));
  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserChildProcessContext(resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::BrowserChildProcessHostFromContext(
                         resource_context));

  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(rph);
  RunInGraphWithRegistry<ProcessContextRegistry>(
      [&](const ProcessContextRegistry* registry) {
        ASSERT_TRUE(process_node);
        EXPECT_EQ(render_process_context, process_node->GetResourceContext());
        EXPECT_EQ(process_node.get(),
                  registry->GetProcessNodeForContext(render_process_context));
        EXPECT_EQ(process_node.get(),
                  registry->GetProcessNodeForContext(resource_context));
      });

  // Make sure the b.com renderer process gets a different token than a.com.
  auto* rph2 =
      content::RenderProcessHost::FromID(render_process_id_b_.GetUnsafeValue());
  ASSERT_TRUE(rph2);
  EXPECT_NE(rph, rph2);
  absl::optional<ProcessContext> context_from_rph2 =
      ProcessContextRegistry::ContextForRenderProcessHost(rph2);
  EXPECT_NE(context_from_rph2, context_from_rph);
  EXPECT_EQ(context_from_rph2,
            ProcessContextRegistry::ContextForRenderProcessHostId(
                render_process_id_b_));

  DeleteNodes();

  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForRenderProcessHostId(
                render_process_id_a_));
  EXPECT_FALSE(
      ProcessContextRegistry::IsRenderProcessContext(render_process_context));
  EXPECT_FALSE(
      ProcessContextRegistry::IsRenderProcessContext(resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::RenderProcessHostFromContext(
                         render_process_context));
  RunInGraphWithRegistry<ProcessContextRegistry>(
      [&](const ProcessContextRegistry* registry) {
        EXPECT_FALSE(process_node);
        EXPECT_EQ(nullptr,
                  registry->GetProcessNodeForContext(render_process_context));
        EXPECT_EQ(nullptr,
                  registry->GetProcessNodeForContext(resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(ProcessContextRegistryTest, BrowserChildProcessContext) {
  CreateNodes();

  ASSERT_TRUE(utility_process_);
  ASSERT_TRUE(utility_process_->host());
  const BrowserChildProcessHostId utility_process_id = utility_process_->id();
  ASSERT_FALSE(utility_process_id.is_null());

  absl::optional<ProcessContext> context_from_utility_host =
      ProcessContextRegistry::ContextForBrowserChildProcessHost(
          utility_process_->host());
  EXPECT_EQ(context_from_utility_host,
            ProcessContextRegistry::ContextForBrowserChildProcessHostId(
                utility_process_id));

  ASSERT_TRUE(context_from_utility_host.has_value());
  const ProcessContext utility_process_context =
      context_from_utility_host.value();
  const ResourceContext resource_context = utility_process_context;
  EXPECT_TRUE(ProcessContextRegistry::IsBrowserChildProcessContext(
      utility_process_context));
  EXPECT_TRUE(
      ProcessContextRegistry::IsBrowserChildProcessContext(resource_context));
  EXPECT_EQ(utility_process_->host(),
            ProcessContextRegistry::BrowserChildProcessHostFromContext(
                utility_process_context));
  EXPECT_EQ(utility_process_->host(),
            ProcessContextRegistry::BrowserChildProcessHostFromContext(
                resource_context));

  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserProcessContext(resource_context));
  EXPECT_FALSE(
      ProcessContextRegistry::IsRenderProcessContext(resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::RenderProcessHostFromContext(
                         resource_context));

  RunInGraphWithRegistry<ProcessContextRegistry>(
      [&](const ProcessContextRegistry* registry) {
        ASSERT_TRUE(weak_utility_process_node_);
        EXPECT_EQ(utility_process_context,
                  weak_utility_process_node_->GetResourceContext());
        EXPECT_EQ(weak_utility_process_node_.get(),
                  registry->GetProcessNodeForContext(utility_process_context));
        EXPECT_EQ(weak_utility_process_node_.get(),
                  registry->GetProcessNodeForContext(resource_context));
      });

  DeleteNodes();

  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForBrowserChildProcessHostId(
                utility_process_id));
  EXPECT_FALSE(ProcessContextRegistry::IsBrowserChildProcessContext(
      utility_process_context));
  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserChildProcessContext(resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::BrowserChildProcessHostFromContext(
                         utility_process_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::BrowserChildProcessHostFromContext(
                         resource_context));
  RunInGraphWithRegistry<ProcessContextRegistry>(
      [&](const ProcessContextRegistry* registry) {
        EXPECT_FALSE(weak_utility_process_node_);
        EXPECT_EQ(nullptr,
                  registry->GetProcessNodeForContext(utility_process_context));
        EXPECT_EQ(nullptr,
                  registry->GetProcessNodeForContext(resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(ProcessContextRegistryTest, InvalidProcessContexts) {
  constexpr auto kInvalidId1 =
      RenderProcessHostId(content::ChildProcessHost::kInvalidUniqueID);
  constexpr auto kInvalidId2 = RenderProcessHostId(0);
  constexpr auto kInvalidId3 =
      BrowserChildProcessHostId(content::ChildProcessHost::kInvalidUniqueID);
  constexpr auto kInvalidId4 = BrowserChildProcessHostId(0);

  // CreateNodes() isn't called so there's no browser ProcessNode.
  EXPECT_EQ(absl::nullopt, ProcessContextRegistry::BrowserProcessContext());
  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForRenderProcessHost(nullptr));
  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForRenderProcessHostId(kInvalidId1));
  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForRenderProcessHostId(kInvalidId2));
  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForBrowserChildProcessHost(nullptr));
  EXPECT_EQ(
      absl::nullopt,
      ProcessContextRegistry::ContextForBrowserChildProcessHostId(kInvalidId3));
  EXPECT_EQ(
      absl::nullopt,
      ProcessContextRegistry::ContextForBrowserChildProcessHostId(kInvalidId4));

  // Find a non-ProcessNode ResourceContext.
  const ResourceContext invalid_resource_context = GetWebContentsPageContext();
  EXPECT_FALSE(ProcessContextRegistry::IsBrowserProcessContext(
      invalid_resource_context));
  EXPECT_FALSE(
      ProcessContextRegistry::IsRenderProcessContext(invalid_resource_context));
  EXPECT_FALSE(ProcessContextRegistry::IsBrowserChildProcessContext(
      invalid_resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::RenderProcessHostFromContext(
                         invalid_resource_context));
  EXPECT_EQ(nullptr, ProcessContextRegistry::BrowserChildProcessHostFromContext(
                         invalid_resource_context));
  RunInGraphWithRegistry<ProcessContextRegistry>(
      [&](const ProcessContextRegistry* registry) {
        EXPECT_EQ(nullptr,
                  registry->GetProcessNodeForContext(invalid_resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(ProcessContextRegistryDisabledTest, UIThreadAccess) {
  CreateNodes();

  ASSERT_TRUE(utility_process_);
  ASSERT_TRUE(utility_process_->host());
  ASSERT_FALSE(utility_process_->id().is_null());

  // Static accessors should safely return null if ProcessContextRegistry is not
  // enabled in Performance Manager.
  EXPECT_EQ(absl::nullopt, ProcessContextRegistry::BrowserProcessContext());
  EXPECT_EQ(absl::nullopt, ProcessContextRegistry::ContextForRenderProcessHost(
                               content::RenderProcessHost::FromID(
                                   render_process_id_a_.GetUnsafeValue())));
  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForRenderProcessHostId(
                render_process_id_a_));
  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForBrowserChildProcessHost(
                utility_process_->host()));
  EXPECT_EQ(absl::nullopt,
            ProcessContextRegistry::ContextForBrowserChildProcessHostId(
                utility_process_->id()));

  const auto kDummyProcessContext = ProcessContext();
  const ResourceContext kDummyResourceContext = kDummyProcessContext;

  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserProcessContext(kDummyProcessContext));
  EXPECT_FALSE(
      ProcessContextRegistry::IsBrowserProcessContext(kDummyResourceContext));
  EXPECT_FALSE(
      ProcessContextRegistry::IsRenderProcessContext(kDummyProcessContext));
  EXPECT_FALSE(
      ProcessContextRegistry::IsRenderProcessContext(kDummyResourceContext));
  EXPECT_EQ(nullptr, ProcessContextRegistry::RenderProcessHostFromContext(
                         kDummyProcessContext));
  EXPECT_EQ(nullptr, ProcessContextRegistry::RenderProcessHostFromContext(
                         kDummyResourceContext));
  EXPECT_FALSE(ProcessContextRegistry::IsBrowserChildProcessContext(
      kDummyProcessContext));
  EXPECT_FALSE(ProcessContextRegistry::IsBrowserChildProcessContext(
      kDummyResourceContext));
  EXPECT_EQ(nullptr, ProcessContextRegistry::BrowserChildProcessHostFromContext(
                         kDummyProcessContext));
  EXPECT_EQ(nullptr, ProcessContextRegistry::BrowserChildProcessHostFromContext(
                         kDummyResourceContext));
}

}  // namespace

}  // namespace performance_manager::resource_attribution
