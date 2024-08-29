// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_page_decorator.h"

#include <memory>
#include <utility>

#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {

class MockObserver : public TabPageObserver {
 public:
  MOCK_METHOD(void, OnTabAdded, (TabPageDecorator::TabHandle*), (override));
  MOCK_METHOD(void,
              OnTabAboutToBeDiscarded,
              (const PageNode*, TabPageDecorator::TabHandle*),
              (override));
  MOCK_METHOD(void,
              OnBeforeTabRemoved,
              (TabPageDecorator::TabHandle*),
              (override));
};

constexpr auto TabHandleMatches = [](const PageNode* handle) {
  return [handle](const TabPageDecorator::TabHandle* tab_handle) {
    return tab_handle->page_node() == handle;
  };
};

class TabPageDecoratorTest : public GraphTestHarness {
 protected:
  void SetUp() override {
    GraphTestHarness::SetUp();
    auto decorator = std::make_unique<TabPageDecorator>();
    observer_ = std::make_unique<MockObserver>();
    decorator->AddObserver(observer_.get());

    graph()->PassToGraph(std::move(decorator));
  }

  std::unique_ptr<MockObserver> observer_;
};

TEST_F(TabPageDecoratorTest, TestBecomesTabAndRemoval) {
  using ::testing::Truly;

  ::testing::InSequence seq;

  MockSinglePageInSingleProcessGraph mock_graph(graph());

  EXPECT_CALL(*observer_,
              OnTabAdded(Truly(TabHandleMatches(mock_graph.page.get()))))
      .Times(1);
  EXPECT_CALL(*observer_, OnBeforeTabRemoved(
                              Truly(TabHandleMatches(mock_graph.page.get()))))
      .Times(1);

  EXPECT_EQ(TabPageDecorator::FromPageNode(mock_graph.page.get()), nullptr);

  mock_graph.page->SetType(PageType::kTab);

  TabPageDecorator::TabHandle* handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  EXPECT_NE(handle, nullptr);
  EXPECT_EQ(handle->page_node(), mock_graph.page.get());

  mock_graph.frame.reset();
  mock_graph.page.reset();
}

TEST_F(TabPageDecoratorTest, TestDiscarding) {
  using ::testing::Truly;

  ::testing::InSequence seq;

  MockSinglePageInSingleProcessGraph mock_graph(graph());

  EXPECT_CALL(*observer_,
              OnTabAdded(Truly(TabHandleMatches(mock_graph.page.get()))))
      .Times(1);

  EXPECT_EQ(TabPageDecorator::FromPageNode(mock_graph.page.get()), nullptr);

  mock_graph.page->SetType(PageType::kTab);

  TabPageDecorator::TabHandle* handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  EXPECT_NE(handle, nullptr);
  EXPECT_EQ(handle->page_node(), mock_graph.page.get());

  auto new_page_node = TestNodeWrapper<PageNodeImpl>::Create(graph());
  EXPECT_CALL(*observer_, OnTabAboutToBeDiscarded(
                              mock_graph.page.get(),
                              Truly(TabHandleMatches(new_page_node.get()))))
      .Times(1);
  EXPECT_CALL(*observer_,
              OnBeforeTabRemoved(Truly(TabHandleMatches(new_page_node.get()))))
      .Times(1);

  mock_graph.page->OnAboutToBeDiscarded(new_page_node->GetWeakPtr());

  mock_graph.frame.reset();
  mock_graph.page.reset();
}

}  // namespace performance_manager
