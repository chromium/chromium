// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class NodeBaseTest : public GraphTestHarness {};

using NodeBaseDeathTest = NodeBaseTest;

}  // namespace

TEST_F(NodeBaseTest, GetAssociatedNodesForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  auto pages_associated_with_process =
      GraphImplOperations::GetAssociatedPageNodes(mock_graph.process.get());
  EXPECT_EQ(1u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(mock_graph.page.get()));

  auto processes_associated_with_page =
      GraphImplOperations::GetAssociatedProcessNodes(mock_graph.page.get());
  EXPECT_EQ(1u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(mock_graph.process.get()));
}

TEST_F(NodeBaseTest, GetAssociatedNodesForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  auto pages_associated_with_process =
      GraphImplOperations::GetAssociatedPageNodes(mock_graph.process.get());
  EXPECT_EQ(2u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(mock_graph.page.get()));
  EXPECT_EQ(1u,
            pages_associated_with_process.count(mock_graph.other_page.get()));

  auto processes_associated_with_page =
      GraphImplOperations::GetAssociatedProcessNodes(mock_graph.page.get());
  EXPECT_EQ(1u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(mock_graph.process.get()));

  auto processes_associated_with_other_page =
      GraphImplOperations::GetAssociatedProcessNodes(
          mock_graph.other_page.get());
  EXPECT_EQ(1u, processes_associated_with_other_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(mock_graph.process.get()));
}

TEST_F(NodeBaseTest, GetAssociatedNodesForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  auto pages_associated_with_process =
      GraphImplOperations::GetAssociatedPageNodes(mock_graph.process.get());
  EXPECT_EQ(1u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(mock_graph.page.get()));

  auto pages_associated_with_other_process =
      GraphImplOperations::GetAssociatedPageNodes(
          mock_graph.other_process.get());
  EXPECT_EQ(1u, pages_associated_with_other_process.size());
  EXPECT_EQ(1u,
            pages_associated_with_other_process.count(mock_graph.page.get()));

  auto processes_associated_with_page =
      GraphImplOperations::GetAssociatedProcessNodes(mock_graph.page.get());
  EXPECT_EQ(2u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(mock_graph.process.get()));
  EXPECT_EQ(
      1u, processes_associated_with_page.count(mock_graph.other_process.get()));
}

TEST_F(NodeBaseTest, GetAssociatedNodesForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());

  auto pages_associated_with_process =
      GraphImplOperations::GetAssociatedPageNodes(mock_graph.process.get());
  EXPECT_EQ(2u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(mock_graph.page.get()));
  EXPECT_EQ(1u,
            pages_associated_with_process.count(mock_graph.other_page.get()));

  auto pages_associated_with_other_process =
      GraphImplOperations::GetAssociatedPageNodes(
          mock_graph.other_process.get());
  EXPECT_EQ(1u, pages_associated_with_other_process.size());
  EXPECT_EQ(1u, pages_associated_with_other_process.count(
                    mock_graph.other_page.get()));

  auto processes_associated_with_page =
      GraphImplOperations::GetAssociatedProcessNodes(mock_graph.page.get());
  EXPECT_EQ(1u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(mock_graph.process.get()));

  auto processes_associated_with_other_page =
      GraphImplOperations::GetAssociatedProcessNodes(
          mock_graph.other_page.get());
  EXPECT_EQ(2u, processes_associated_with_other_page.size());
  EXPECT_EQ(
      1u, processes_associated_with_other_page.count(mock_graph.process.get()));
  EXPECT_EQ(1u, processes_associated_with_other_page.count(
                    mock_graph.other_process.get()));
}

}  // namespace performance_manager
