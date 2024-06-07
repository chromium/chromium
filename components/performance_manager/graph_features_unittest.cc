// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/graph_features.h"

#include "build/build_config.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class GraphFeaturesTest : public ::testing::Test {
 protected:
  // Some features require browser threads.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GraphFeaturesTest, ConfigureGraph) {
  GraphFeatures features;

  EXPECT_FALSE(features.flags().v8_context_tracker);
  features.EnableV8ContextTracker();
  EXPECT_TRUE(features.flags().v8_context_tracker);

  TestGraphImpl graph;
  graph.SetUp();

  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));
  features.ConfigureGraph(&graph);
  EXPECT_TRUE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  graph.TearDown();
}

TEST_F(GraphFeaturesTest, EnableDefault) {
  GraphFeatures features;
  TestGraphImpl graph;
  graph.SetUp();

  // The ExecutionContextRegistry is a permanent graph-registered object.
  EXPECT_TRUE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));

  EXPECT_EQ(1u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(0u, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(0u, graph.NodeDataDescriberCountForTesting());
  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  // An empty config should install nothing.
  features.ConfigureGraph(&graph);
  EXPECT_EQ(1u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(0u, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(0u, graph.NodeDataDescriberCountForTesting());
  EXPECT_TRUE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  // Validate that the default configuration works as expected.
  features.EnableDefault();
  features.ConfigureGraph(&graph);

  // Ensure the GraphRegistered objects can be queried directly.
  EXPECT_TRUE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_TRUE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  graph.TearDown();
}

TEST_F(GraphFeaturesTest, StandardConfigurations) {
  GraphFeatures features;
  EXPECT_EQ(features.flags().flags, GraphFeatures::WithNone().flags().flags);
  features.EnableMinimal();
  EXPECT_EQ(features.flags().flags, GraphFeatures::WithMinimal().flags().flags);

  // This test will fail if Default is not a superset of Minimal, since it does
  // not remove the Minimal flags. That's a good thing to test since it would
  // be confusing for Minimal to include features that Default doesn't.
  features.EnableDefault();
  EXPECT_EQ(features.flags().flags, GraphFeatures::WithDefault().flags().flags);
}

}  // namespace performance_manager
