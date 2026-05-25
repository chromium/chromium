// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_page_decorator.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/common/content_features.h"
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
  EXPECT_EQ(TabPageDecorator::WeakHandleFromPageNode(mock_graph.page.get()),
            nullptr);

  mock_graph.page->SetType(PageType::kTab);

  TabPageDecorator::TabHandle* handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  EXPECT_NE(handle, nullptr);
  EXPECT_EQ(handle->page_node(), mock_graph.page.get());

  base::WeakPtr<TabPageDecorator::TabHandle> weak_handle =
      TabPageDecorator::WeakHandleFromPageNode(mock_graph.page.get());
  EXPECT_EQ(weak_handle.get(), handle);

  mock_graph.frame.reset();
  mock_graph.page.reset();
  EXPECT_FALSE(weak_handle);
}

TEST_F(TabPageDecoratorTest, TestDiscarding) {
  using ::testing::Truly;

  ::testing::InSequence seq;

  MockSinglePageInSingleProcessGraph mock_graph(graph());

  EXPECT_CALL(*observer_,
              OnTabAdded(Truly(TabHandleMatches(mock_graph.page.get()))))
      .Times(1);

  EXPECT_EQ(TabPageDecorator::FromPageNode(mock_graph.page.get()), nullptr);
  EXPECT_EQ(TabPageDecorator::WeakHandleFromPageNode(mock_graph.page.get()),
            nullptr);

  mock_graph.page->SetType(PageType::kTab);

  TabPageDecorator::TabHandle* handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());
  EXPECT_NE(handle, nullptr);
  EXPECT_EQ(handle->page_node(), mock_graph.page.get());

  base::WeakPtr<TabPageDecorator::TabHandle> weak_handle =
      TabPageDecorator::WeakHandleFromPageNode(mock_graph.page.get());
  EXPECT_EQ(weak_handle.get(), handle);

  auto new_page_node = TestNodeWrapper<PageNodeImpl>::Create(graph());
  // When kWebContentsDiscard is enabled, the page node is not replaced.
  auto& page_node_after_discard =
      base::FeatureList::IsEnabled(::features::kWebContentsDiscard)
          ? mock_graph.page
          : new_page_node;

  EXPECT_CALL(*observer_,
              OnTabAboutToBeDiscarded(
                  mock_graph.page.get(),
                  Truly(TabHandleMatches(page_node_after_discard.get()))))
      .Times(1);
  EXPECT_CALL(*observer_, OnBeforeTabRemoved(Truly(
                              TabHandleMatches(page_node_after_discard.get()))))
      .Times(1);

  mock_graph.page->OnAboutToBeDiscarded(page_node_after_discard->GetWeakPtr());

  // WeakPtr should not be reset during discard.
  EXPECT_TRUE(weak_handle);

  mock_graph.frame.reset();
  mock_graph.page.reset();
  if (base::FeatureList::IsEnabled(::features::kWebContentsDiscard)) {
    // PageNode doesn't change during discard so WeakPtr is reset now.
    EXPECT_FALSE(weak_handle);
  } else {
    EXPECT_TRUE(weak_handle);
    new_page_node.reset();
    EXPECT_FALSE(weak_handle);
  }
}

}  // namespace performance_manager
