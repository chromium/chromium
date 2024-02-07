// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/resource_contexts.h"

#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_attribution {

namespace {

template <typename PublicNode, typename NodeImpl>
const PublicNode* ToPublic(
    const performance_manager::TestNodeWrapper<NodeImpl>& wrapper) {
  return wrapper.get();
}

using ResourceAttrResourceContextsTest = performance_manager::GraphTestHarness;
using ResourceAttrResourceContextsDeathTest = ResourceAttrResourceContextsTest;

// Tests the context tokens returned from PM nodes.
TEST_F(ResourceAttrResourceContextsTest, NodeContexts) {
  performance_manager::MockUtilityAndMultipleRenderProcessesGraph mock_graph(
      graph());

  // Test each type of ProcessNode (browser, renderer, non-renderer child) since
  // they use different constructors.

  // Ensure each node gets a fresh token.
  const std::set<FrameContext> frame_contexts{
      mock_graph.frame->GetResourceContext(),
      mock_graph.other_frame->GetResourceContext(),
      mock_graph.child_frame->GetResourceContext(),
  };
  EXPECT_EQ(frame_contexts.size(), 3u);
  EXPECT_NE(mock_graph.page->GetResourceContext(),
            mock_graph.other_page->GetResourceContext());
  const std::set<ProcessContext> process_contexts{
      mock_graph.browser_process->GetResourceContext(),
      mock_graph.process->GetResourceContext(),
      mock_graph.other_process->GetResourceContext(),
      mock_graph.utility_process->GetResourceContext(),
  };
  EXPECT_EQ(process_contexts.size(), 4u);
  EXPECT_NE(mock_graph.worker->GetResourceContext(),
            mock_graph.other_worker->GetResourceContext());
}

TEST_F(ResourceAttrResourceContextsTest, ResourceContextComparators) {
  performance_manager::MockMultiplePagesAndWorkersWithMultipleProcessesGraph
      mock_graph(graph());

  // Ensure tokens of the same type can be compared when wrapped in
  // ResourceContext.
  const ResourceContext process_context =
      mock_graph.process->GetResourceContext();
  const ResourceContext other_process_context =
      mock_graph.other_process->GetResourceContext();

  // ResourceContext == node context
  EXPECT_EQ(process_context, mock_graph.process->GetResourceContext());
  // node context == ResourceContext
  EXPECT_EQ(mock_graph.other_process->GetResourceContext(),
            other_process_context);
  // ResourceContext == ResourceContext
  const ResourceContext process_context_copy = process_context;
  EXPECT_EQ(process_context, process_context_copy);

  // ResourceContext != node context
  EXPECT_NE(process_context, mock_graph.other_process->GetResourceContext());
  // node context != ResourceContext
  EXPECT_NE(mock_graph.process->GetResourceContext(), other_process_context);
  // ResourceContext != ResourceContext
  EXPECT_NE(process_context, other_process_context);

  // Ensure tokens of different types can be compared when wrapped in
  // ResourceContext, although they'll never be equal.
  const ResourceContext page_context = mock_graph.page->GetResourceContext();

  // ResourceContext != node context
  EXPECT_NE(process_context, mock_graph.page->GetResourceContext());
  // node context != Resource Context
  EXPECT_NE(mock_graph.process->GetResourceContext(), page_context);
  // ResourceContext != ResourceContext
  EXPECT_NE(process_context, page_context);
}

TEST_F(ResourceAttrResourceContextsTest, ResourceContextConverters) {
  performance_manager::MockMultiplePagesAndWorkersWithMultipleProcessesGraph
      mock_graph(graph());

  const ResourceContext process_context =
      mock_graph.process->GetResourceContext();
  const ResourceContext page_context = mock_graph.page->GetResourceContext();

  EXPECT_TRUE(ContextIs<ProcessContext>(process_context));
  EXPECT_FALSE(ContextIs<ProcessContext>(page_context));

  const ProcessContext unwrapped_process_context =
      AsContext<ProcessContext>(process_context);
  EXPECT_EQ(unwrapped_process_context,
            mock_graph.process->GetResourceContext());

  EXPECT_THAT(AsOptionalContext<ProcessContext>(process_context),
              ::testing::Optional(mock_graph.process->GetResourceContext()));
  EXPECT_EQ(AsOptionalContext<ProcessContext>(page_context), std::nullopt);
}

TEST_F(ResourceAttrResourceContextsTest, ResourceContextTypeId) {
  using ResourceContextTypeId = internal::ResourceContextTypeId;

  performance_manager::MockMultiplePagesAndWorkersWithMultipleProcessesGraph
      mock_graph(graph());

  const ResourceContext process_context =
      mock_graph.process->GetResourceContext();
  const ResourceContext page_context = mock_graph.page->GetResourceContext();

  EXPECT_EQ(ResourceContextTypeId(process_context),
            ResourceContextTypeId::ForType<ProcessContext>());
  EXPECT_EQ(ResourceContextTypeId(page_context),
            ResourceContextTypeId::ForType<PageContext>());

  // Different types get different id's.
  EXPECT_NE(ResourceContextTypeId(process_context),
            ResourceContextTypeId(page_context));

  // Different instances of the same type get the same id.
  EXPECT_EQ(ResourceContextTypeId(page_context),
            ResourceContextTypeId(mock_graph.other_page->GetResourceContext()));
}

TEST_F(ResourceAttrResourceContextsDeathTest, FailedResourceContextConverters) {
  performance_manager::MockMultiplePagesAndWorkersWithMultipleProcessesGraph
      mock_graph(graph());
  const ResourceContext page_context = mock_graph.page->GetResourceContext();
  EXPECT_DEATH_IF_SUPPORTED(AsContext<ProcessContext>(page_context),
                            "Bad variant access");
}

}  // namespace

}  // namespace resource_attribution
