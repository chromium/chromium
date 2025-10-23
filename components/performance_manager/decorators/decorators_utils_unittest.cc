// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/decorators_utils.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class FakePageNodeDecoratorData
    : public NodeAttachedDataImpl<FakePageNodeDecoratorData> {
 public:
  explicit FakePageNodeDecoratorData(const PageNodeImpl* page_node) {}

  FakePageNodeDecoratorData() = default;
  ~FakePageNodeDecoratorData() override = default;
  FakePageNodeDecoratorData(const FakePageNodeDecoratorData& other) = delete;
  FakePageNodeDecoratorData& operator=(const FakePageNodeDecoratorData&) =
      delete;

  int value() const { return value_; }

  void SetProperty(int value) { value_ = value; }

 private:
  int value_ = 0;
};

class SetPropertyDuringDestructionObserver final : public PageNodeObserver {
 public:
  explicit SetPropertyDuringDestructionObserver(const PageNode* page_node)
      : contents_(page_node->GetWebContents()) {
    observation_.Observe(page_node->GetGraph());
  }

  ~SetPropertyDuringDestructionObserver() final = default;

  SetPropertyDuringDestructionObserver(
      const SetPropertyDuringDestructionObserver&) = delete;
  SetPropertyDuringDestructionObserver& operator=(
      const SetPropertyDuringDestructionObserver&) = delete;

  // Returns true if OnPageNodeRemoved was called for the observed PageNode.
  bool page_node_removed() const { return page_node_removed_; }

 private:
  // PageNodeObserver:
  void OnPageNodeRemoved(const PageNode* page_node) final {
    ASSERT_TRUE(contents_);
    if (contents_.get() != page_node->GetWebContents().get()) {
      // Wrong PageNode.
      return;
    }

    // The PageNode is no longer in the graph, so can't be looked up from the
    // WebContents. The WebContents itself should still exist because this is
    // invoked synchronously from WebContentsObserver::WebContentsDestroyed.
    EXPECT_FALSE(
        PerformanceManager::GetPrimaryPageNodeForWebContents(contents_.get()));

    // The test passes if this doesn't crash.
    SetPropertyForWebContentsPageNode(
        contents_.get(), &FakePageNodeDecoratorData::SetProperty, 9999);
    observation_.Reset();
    page_node_removed_ = true;
  }

  base::ScopedObservation<Graph, PageNodeObserver> observation_{this};
  base::WeakPtr<content::WebContents> contents_;
  bool page_node_removed_ = false;
};

class DecoratorsUtilsTest : public PerformanceManagerTestHarness {
 public:
  using Super = PerformanceManagerTestHarness;

  void SetUp() override {
    Super::SetUp();
    SetContents(CreateTestWebContents());
  }

  void TearDown() override {
    DeleteContents();
    Super::TearDown();
  }
};

}  // namespace

// Test that the function parameter for SetPropertyForWebContentsPageNode has
// been called.
TEST_F(DecoratorsUtilsTest, SetPropertyForWebContentsPageNode) {
  constexpr int kFakePropertyValue = 1234;

  // Set up and create a dummy PageNode.
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(node);

  // Call to the tested function with SetProperty passed in as argument.
  SetPropertyForWebContentsPageNode(web_contents(),
                                    &FakePageNodeDecoratorData::SetProperty,
                                    kFakePropertyValue);

  auto* data = FakePageNodeDecoratorData::Get(node.get());
  ASSERT_TRUE(data);
  EXPECT_EQ(data->value(), kFakePropertyValue);
}

TEST_F(DecoratorsUtilsTest, SetPropertyForWebContentsPageNodeDuringDelete) {
  // Set up and create a dummy PageNode.
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(node);

  SetPropertyDuringDestructionObserver destruction_observer(node.get());

  // Destroy the WebContents. The test passes if `destruction_observer`
  // calls SetPropertyForWebContentsPageNode without crashing.
  DeleteContents();
  EXPECT_TRUE(destruction_observer.page_node_removed());
}

}  // namespace performance_manager
