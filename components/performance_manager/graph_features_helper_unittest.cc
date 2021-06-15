// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/graph_features_helper.h"

#include "build/build_config.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(GraphFeaturesHelperTest, ConfigureGraph) {
  GraphFeaturesHelper features;

  EXPECT_FALSE(features.flags().execution_context_registry);
  EXPECT_FALSE(features.flags().v8_context_tracker);
  features.EnableV8ContextTracker();
  EXPECT_TRUE(features.flags().execution_context_registry);
  EXPECT_TRUE(features.flags().v8_context_tracker);

  TestGraphImpl graph;
  graph.SetUp();

  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));
  features.ConfigureGraph(&graph);
  EXPECT_TRUE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  graph.TearDown();
}

TEST(GraphFeaturesHelperTest, EnableDefault) {
  GraphFeaturesHelper features;
  TestGraphImpl graph;
  graph.SetUp();

  EXPECT_EQ(0u, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(0u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(0u, graph.NodeDataDescriberCountForTesting());
  EXPECT_FALSE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  // An empty config should install nothing.
  features.ConfigureGraph(&graph);
  EXPECT_EQ(0u, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(0u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(0u, graph.NodeDataDescriberCountForTesting());
  EXPECT_FALSE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  size_t graph_owned_count = 11;
#if !defined(OS_ANDROID)
  // The SiteDataRecorder is not available on Android.
  graph_owned_count++;
#endif

  // Validate that the default configuration works as expected.
  features.EnableDefault();
  features.ConfigureGraph(&graph);
  EXPECT_EQ(graph_owned_count, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(3u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(9u, graph.NodeDataDescriberCountForTesting());
  // Ensure the GraphRegistered objects can be queried directly.
  EXPECT_TRUE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_TRUE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  graph.TearDown();
}

}  // namespace performance_manager
