// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/resource_contexts.h"

#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::resource_attribution {

namespace {

template <typename PublicNode, typename NodeImpl>
const PublicNode* ToPublic(const TestNodeWrapper<NodeImpl>& wrapper) {
  return wrapper.get();
}

using ResourceContextTest = GraphTestHarness;
using ResourceContextDeathTest = ResourceContextTest;

// Tests the context tokens returned from PM nodes.
TEST_F(ResourceContextTest, NodeContexts) {
  MockUtilityAndMultipleRenderProcessesGraph mock_graph(graph());

  // Test each type of ProcessNode (browser, renderer, non-renderer child) since
  // they use different constructors.

  // Ensure the public and private accessors return the same value.
  EXPECT_EQ(ToPublic<FrameNode>(mock_graph.frame)->GetResourceContext(),
            mock_graph.frame->resource_context());
  EXPECT_EQ(ToPublic<PageNode>(mock_graph.page)->GetResourceContext(),
            mock_graph.page->resource_context());
  EXPECT_EQ(
      ToPublic<ProcessNode>(mock_graph.browser_process)->GetResourceContext(),
      mock_graph.browser_process->resource_context());
  EXPECT_EQ(ToPublic<ProcessNode>(mock_graph.process)->GetResourceContext(),
            mock_graph.process->resource_context());
  EXPECT_EQ(
      ToPublic<ProcessNode>(mock_graph.utility_process)->GetResourceContext(),
      mock_graph.utility_process->resource_context());
  EXPECT_EQ(ToPublic<WorkerNode>(mock_graph.worker)->GetResourceContext(),
            mock_graph.worker->resource_context());

  // Ensure each node gets a fresh token.
  const std::set<FrameContext> frame_contexts{
      mock_graph.frame->resource_context(),
      mock_graph.other_frame->resource_context(),
      mock_graph.child_frame->resource_context(),
  };
  EXPECT_EQ(frame_contexts.size(), 3u);
  EXPECT_NE(mock_graph.page->resource_context(),
            mock_graph.other_page->resource_context());
  const std::set<ProcessContext> process_contexts{
      mock_graph.browser_process->resource_context(),
      mock_graph.process->resource_context(),
      mock_graph.other_process->resource_context(),
      mock_graph.utility_process->resource_context(),
  };
  EXPECT_EQ(process_contexts.size(), 4u);
  EXPECT_NE(mock_graph.worker->resource_context(),
            mock_graph.other_worker->resource_context());
}

TEST_F(ResourceContextTest, ResourceContextComparators) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  // Ensure tokens of the same type can be compared when wrapped in
  // ResourceContext.
  const ResourceContext process_context =
      mock_graph.process->resource_context();
  const ResourceContext other_process_context =
      mock_graph.other_process->resource_context();

  // ResourceContext == node context
  EXPECT_EQ(process_context, mock_graph.process->resource_context());
  // node context == ResourceContext
  EXPECT_EQ(mock_graph.other_process->resource_context(),
            other_process_context);
  // ResourceContext == ResourceContext
  const ResourceContext process_context_copy = process_context;
  EXPECT_EQ(process_context, process_context_copy);

  // ResourceContext != node context
  EXPECT_NE(process_context, mock_graph.other_process->resource_context());
  // node context != ResourceContext
  EXPECT_NE(mock_graph.process->resource_context(), other_process_context);
  // ResourceContext != ResourceContext
  EXPECT_NE(process_context, other_process_context);

  // Ensure tokens of different types can be compared when wrapped in
  // ResourceContext, although they'll never be equal.
  const ResourceContext page_context = mock_graph.page->resource_context();

  // ResourceContext != node context
  EXPECT_NE(process_context, mock_graph.page->resource_context());
  // node context != Resource Context
  EXPECT_NE(mock_graph.process->resource_context(), page_context);
  // ResourceContext != ResourceContext
  EXPECT_NE(process_context, page_context);
}

TEST_F(ResourceContextTest, ResourceContextConverters) {
  using ::testing::Optional;

  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  const ResourceContext process_context =
      mock_graph.process->resource_context();
  const ResourceContext page_context = mock_graph.page->resource_context();

  EXPECT_TRUE(ContextIs<ProcessContext>(process_context));
  EXPECT_FALSE(ContextIs<ProcessContext>(page_context));

  const ProcessContext unwrapped_process_context =
      AsContext<ProcessContext>(process_context);
  EXPECT_EQ(unwrapped_process_context, mock_graph.process->resource_context());

  EXPECT_THAT(AsOptionalContext<ProcessContext>(process_context),
              Optional(mock_graph.process->resource_context()));
  EXPECT_EQ(AsOptionalContext<ProcessContext>(page_context), absl::nullopt);
}

TEST_F(ResourceContextDeathTest, FailedResourceContextConverters) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());
  const ResourceContext page_context = mock_graph.page->resource_context();
  EXPECT_DEATH_IF_SUPPORTED(AsContext<ProcessContext>(page_context),
                            "Bad variant access");
}

}  // namespace

}  // namespace performance_manager::resource_attribution
