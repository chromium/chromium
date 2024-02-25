// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_connectedness_decorator.h"

#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"

namespace performance_manager {

class TabConnectednessDecoratorTest : public GraphTestHarness {
 protected:
  void SetUp() override {
    GetGraphFeatures().EnableTabConnectednessDecorator();
    GraphTestHarness::SetUp();
  }

  TabConnectednessDecorator* decorator() {
    return graph()->GetRegisteredObjectAs<TabConnectednessDecorator>();
  }

  void SwitchTab(const PageNode* from, const PageNode* to) {
    decorator()->OnTabSwitch(from, to);
  }

  void CheckConnectedness(const PageNode* from,
                          const PageNode* to,
                          float expected_connectedness) {
    TabPageDecorator::TabHandle* first_handle =
        TabPageDecorator::FromPageNode(from);
    CHECK(first_handle);
    TabPageDecorator::TabHandle* second_handle =
        TabPageDecorator::FromPageNode(to);
    CHECK(second_handle);

    EXPECT_EQ(
        decorator()->ComputeConnectednessBetween(first_handle, second_handle),
        expected_connectedness);
  }
};

TEST_F(TabConnectednessDecoratorTest, TestNotConnected) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetType(PageType::kTab);
  mock_graph.other_page->SetType(PageType::kTab);

  TabPageDecorator::TabHandle* first_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  CHECK(first_handle);
  TabPageDecorator::TabHandle* second_handle =
      TabPageDecorator::FromPageNode(mock_graph.other_page.get());
  CHECK(second_handle);

  EXPECT_EQ(
      decorator()->ComputeConnectednessBetween(first_handle, second_handle),
      0.0f);
}

TEST_F(TabConnectednessDecoratorTest, TestOnlyConnectedToSingleOtherTab) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetType(PageType::kTab);
  mock_graph.other_page->SetType(PageType::kTab);

  SwitchTab(mock_graph.page.get(), mock_graph.other_page.get());

  TabPageDecorator::TabHandle* first_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  CHECK(first_handle);
  TabPageDecorator::TabHandle* second_handle =
      TabPageDecorator::FromPageNode(mock_graph.other_page.get());
  CHECK(second_handle);

  EXPECT_EQ(
      decorator()->ComputeConnectednessBetween(first_handle, second_handle),
      1.0f);
  // Connectedness is undirected, so c(a, b) != c(b, a)
  EXPECT_EQ(
      decorator()->ComputeConnectednessBetween(second_handle, first_handle),
      0.0f);

  // Performing another switch from/to the same tabs doesn't change the
  // connectedness. Because that's the only connection in the graph, it's
  // still 1.0.
  SwitchTab(mock_graph.page.get(), mock_graph.other_page.get());
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_page.get(), 1.0f);
}

TEST_F(TabConnectednessDecoratorTest, TestConnectedToMultipleTabs) {
  // This graph has 4 pages: `page` and `other_pages[0..2]`, hereafter referred
  // to as `OP_N`
  MockManyPagesInSingleProcessGraph mock_graph(graph(), 3);

  mock_graph.page->SetType(PageType::kTab);
  for (auto& p : mock_graph.other_pages) {
    p->SetType(PageType::kTab);
  }

  // Switch once from `page` to `OP_0`, the connectedness of this relation is 1.
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[0].get());
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[0].get(),
                     1.0f);

  // Switch from `page` to another page, page's connectedness to either that one
  // or `OP_0` is 1/2.
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[1].get());
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[0].get(),
                     0.5f);
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[1].get(),
                     0.5f);

  // Switching again from `page` to the same `OP_1` changes page's connectedness
  // to `OP_0` and `OP_1` to 1/3 and 2/3 respectively.
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[1].get());
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[0].get(),
                     1.0f / 3.0f);
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[1].get(),
                     2.0f / 3.0f);

  // `page` is not connected to `OP_2` because nothing has switched to `OP_2`
  // yet.
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[2].get(),
                     0.0f);

  // Switch from `OP_1` to `OP_0` and `OP_2`: its connectedness to either is now
  // 1/2.
  SwitchTab(mock_graph.other_pages[1].get(), mock_graph.other_pages[0].get());
  SwitchTab(mock_graph.other_pages[1].get(), mock_graph.other_pages[2].get());

  // `page` is connected to `OP_2` through `OP_1`: page -2/3-> OP_1 -0.5-> OP_2.
  // The connectedness is the sum of the weights of each connected path, so
  // `page` has 2/3 * 1/2 connectedness to `OP_2`
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[1].get(),
                     2.0f / 3.0f);
  CheckConnectedness(mock_graph.other_pages[1].get(),
                     mock_graph.other_pages[2].get(), 0.5f);
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[2].get(),
                     2.0f / 3.0f * 0.5f);

  // Switch to `OP_2` directly from `page`, the connectedness should now be the
  // sum of the direct connectedness and the connectedness through `OP_1`, which
  // is now 1/2: 1/4 + 1/2 * 1/2 = 1/2.
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[2].get());
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[1].get(),
                     0.5f);
  CheckConnectedness(mock_graph.other_pages[1].get(),
                     mock_graph.other_pages[2].get(), 0.5f);
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[2].get(),
                     0.5f);
}

TEST_F(TabConnectednessDecoratorTest, TestConnectednessHandlesTabRemoval) {
  // This graph has 4 pages: `page` and `other_pages[0..2]`, hereafter referred
  // to as `OP_N`
  MockManyPagesInSingleProcessGraph mock_graph(graph(), 3);

  mock_graph.page->SetType(PageType::kTab);
  for (auto& p : mock_graph.other_pages) {
    p->SetType(PageType::kTab);
  }

  // Setup a connectedness graph where the score from `page` to `OP_2` is 1/4 +
  // 1/4 * 1/2 = 3/8
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[0].get());
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[0].get());
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[1].get());
  SwitchTab(mock_graph.page.get(), mock_graph.other_pages[2].get());
  SwitchTab(mock_graph.other_pages[1].get(), mock_graph.other_pages[0].get());
  SwitchTab(mock_graph.other_pages[1].get(), mock_graph.other_pages[2].get());
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[2].get(),
                     3.0f / 8.0f);

  // Simulate `OP_1` being closed, removing the connectedness information
  // related to `OP_1`.
  mock_graph.other_frames[1].reset();
  mock_graph.other_pages[1].reset();

  // The connectedness from `page` to `OP_2` is now only its (updated) direct
  // connectedness to `OP_2`: 1/3
  CheckConnectedness(mock_graph.page.get(), mock_graph.other_pages[2].get(),
                     1.0f / 3.0f);
}

TEST_F(TabConnectednessDecoratorTest, TestSwitchToDiscardedTab) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetType(PageType::kTab);
  mock_graph.other_page->SetType(PageType::kTab);

  SwitchTab(mock_graph.page.get(), mock_graph.other_page.get());

  TabPageDecorator::TabHandle* first_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  CHECK(first_handle);
  TabPageDecorator::TabHandle* second_handle =
      TabPageDecorator::FromPageNode(mock_graph.other_page.get());
  CHECK(second_handle);

  auto new_page_node = TestNodeWrapper<PageNodeImpl>::Create(graph());
  mock_graph.other_page->OnAboutToBeDiscarded(new_page_node->GetWeakPtr());

  EXPECT_EQ(
      decorator()->ComputeConnectednessBetween(first_handle, second_handle),
      1.0f);
  // Connectedness is undirected, so c(a, b) != c(b, a)
  EXPECT_EQ(
      decorator()->ComputeConnectednessBetween(second_handle, first_handle),
      0.0f);

  SwitchTab(mock_graph.page.get(), new_page_node.get());
  CheckConnectedness(mock_graph.page.get(), new_page_node.get(), 1.0f);
}

// This test sets up an extreme scenario where tab A is equally connected to
// 2000 other tabs, which are in turn fully (1.0) connected to tab B. In this
// case, Connectedness(A, B) is equal to 1.0, but floating point error
// accumulation makes it so the computation produces roughly 1.00002. Because
// connectedness represents a probability, it must be in the interval [0, 1], so
// this test exercises the code that ensures this property remains true.
TEST_F(TabConnectednessDecoratorTest, TestDoesntReturnGreaterThanOne) {
  MockManyPagesInSingleProcessGraph mock_graph(graph(), 2001);

  mock_graph.page->SetType(PageType::kTab);
  mock_graph.other_pages[0]->SetType(PageType::kTab);
  for (int i = 1; i < 2001; ++i) {
    mock_graph.other_pages[i]->SetType(PageType::kTab);
    SwitchTab(mock_graph.page.get(), mock_graph.other_pages[i].get());
    SwitchTab(mock_graph.other_pages[i].get(), mock_graph.other_pages[0].get());
  }

  TabPageDecorator::TabHandle* first_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  CHECK(first_handle);
  TabPageDecorator::TabHandle* second_handle =
      TabPageDecorator::FromPageNode(mock_graph.other_pages[0].get());
  CHECK(second_handle);

  EXPECT_EQ(
      decorator()->ComputeConnectednessBetween(first_handle, second_handle),
      1.0f);
}

}  // namespace performance_manager
