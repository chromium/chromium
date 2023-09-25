// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/frame_context_registry.h"

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/resource_attribution/registry_browsertest_harness.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::resource_attribution {

namespace {

using FrameContextRegistryTest = RegistryBrowserTestHarness;
using FrameContextRegistryDisabledTest = RegistryDisabledBrowserTestHarness;

IN_PROC_BROWSER_TEST_F(FrameContextRegistryTest, FrameContexts) {
  CreateNodes();

  auto* rfh = content::RenderFrameHost::FromID(main_frame_id_);
  ASSERT_TRUE(rfh);
  absl::optional<FrameContext> context_from_rfh =
      FrameContextRegistry::ContextForRenderFrameHost(rfh);
  EXPECT_EQ(context_from_rfh,
            FrameContextRegistry::ContextForRenderFrameHostId(main_frame_id_));

  ASSERT_TRUE(context_from_rfh.has_value());
  const FrameContext frame_context = context_from_rfh.value();
  const ResourceContext resource_context = frame_context;
  EXPECT_EQ(rfh,
            FrameContextRegistry::RenderFrameHostFromContext(frame_context));
  EXPECT_EQ(rfh,
            FrameContextRegistry::RenderFrameHostFromContext(resource_context));

  base::WeakPtr<FrameNode> frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh);
  RunInGraphWithRegistry<FrameContextRegistry>(
      [&](const FrameContextRegistry* registry) {
        ASSERT_TRUE(frame_node);
        EXPECT_EQ(frame_context, frame_node->GetResourceContext());
        EXPECT_EQ(frame_node.get(),
                  registry->GetFrameNodeForContext(frame_context));
        EXPECT_EQ(frame_node.get(),
                  registry->GetFrameNodeForContext(resource_context));
      });

  // Make sure the b.com frame gets a different token than a.com.
  auto* rfh2 = content::RenderFrameHost::FromID(sub_frame_id_);
  ASSERT_TRUE(rfh2);
  EXPECT_NE(rfh, rfh2);
  absl::optional<FrameContext> context_from_rfh2 =
      FrameContextRegistry::ContextForRenderFrameHost(rfh2);
  EXPECT_NE(context_from_rfh2, context_from_rfh);
  EXPECT_EQ(context_from_rfh2,
            FrameContextRegistry::ContextForRenderFrameHostId(sub_frame_id_));

  DeleteNodes();

  EXPECT_EQ(absl::nullopt,
            FrameContextRegistry::ContextForRenderFrameHostId(main_frame_id_));
  EXPECT_EQ(nullptr,
            FrameContextRegistry::RenderFrameHostFromContext(frame_context));
  RunInGraphWithRegistry<FrameContextRegistry>(
      [&](const FrameContextRegistry* registry) {
        EXPECT_FALSE(frame_node);
        EXPECT_EQ(nullptr, registry->GetFrameNodeForContext(frame_context));
        EXPECT_EQ(nullptr, registry->GetFrameNodeForContext(resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(FrameContextRegistryTest, InvalidFrameContexts) {
  constexpr auto kInvalidId = content::GlobalRenderFrameHostId();

  EXPECT_EQ(absl::nullopt,
            FrameContextRegistry::ContextForRenderFrameHost(nullptr));
  EXPECT_EQ(absl::nullopt,
            FrameContextRegistry::ContextForRenderFrameHostId(kInvalidId));

  // Find a non-FrameNode ResourceContext.
  const ResourceContext invalid_resource_context = GetWebContentsPageContext();
  EXPECT_EQ(nullptr, FrameContextRegistry::RenderFrameHostFromContext(
                         invalid_resource_context));
  RunInGraphWithRegistry<FrameContextRegistry>(
      [&](const FrameContextRegistry* registry) {
        EXPECT_EQ(nullptr,
                  registry->GetFrameNodeForContext(invalid_resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(FrameContextRegistryTest, OnBeforeFrameNodeRemoved) {
  CreateNodes();

  auto* rfh = content::RenderFrameHost::FromID(main_frame_id_);
  ASSERT_TRUE(rfh);
  absl::optional<FrameContext> frame_context =
      FrameContextRegistry::ContextForRenderFrameHost(rfh);
  ASSERT_TRUE(frame_context.has_value());

  RemoveFrameNodeWaiter waiter(
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh),
      base::BindLambdaForTesting([&](const FrameNode* frame_node) {
        // `frame_node` should still be available from FrameContextRegistry in
        // OnBeforeFrameNodeRemoved.
        const auto* registry =
            FrameContextRegistry::GetFromGraph(frame_node->GetGraph());
        ASSERT_TRUE(registry);
        EXPECT_EQ(registry->GetFrameNodeForContext(frame_context.value()),
                  frame_node);
      }));

  DeleteNodes();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(FrameContextRegistryDisabledTest, UIThreadAccess) {
  CreateNodes();

  // Static accessors should safely return null if FrameContextRegistry is not
  // enabled in Performance Manager.
  EXPECT_EQ(absl::nullopt,
            FrameContextRegistry::ContextForRenderFrameHost(
                content::RenderFrameHost::FromID(main_frame_id_)));
  EXPECT_EQ(absl::nullopt,
            FrameContextRegistry::ContextForRenderFrameHostId(main_frame_id_));

  const auto kDummyFrameContext = FrameContext();
  const ResourceContext kDummyResourceContext = kDummyFrameContext;

  EXPECT_EQ(nullptr, FrameContextRegistry::RenderFrameHostFromContext(
                         kDummyFrameContext));
  EXPECT_EQ(nullptr, FrameContextRegistry::RenderFrameHostFromContext(
                         kDummyResourceContext));
}

}  // namespace

}  // namespace performance_manager::resource_attribution
