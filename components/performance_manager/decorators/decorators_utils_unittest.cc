// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/decorators_utils.h"

#include <utility>

#include "base/test/bind.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class FakePageNodeDecoratorData
    : public ExternalNodeAttachedDataImpl<FakePageNodeDecoratorData> {
 public:
  explicit FakePageNodeDecoratorData(const PageNodeImpl* page_node) {}

  FakePageNodeDecoratorData() = default;
  ~FakePageNodeDecoratorData() override = default;
  FakePageNodeDecoratorData(const FakePageNodeDecoratorData& other) = delete;
  FakePageNodeDecoratorData& operator=(const FakePageNodeDecoratorData&) =
      delete;

  void SetOnSetPropertyCalledExpectations(base::OnceClosure closure_to_call,
                                          int expected_value) {
    closure_to_call_ = std::move(closure_to_call);
    expected_value_ = expected_value;
  }
  void SetProperty(int value) {
    EXPECT_EQ(expected_value_, value);
    std::move(closure_to_call_).Run();
  }

 private:
  base::OnceClosure closure_to_call_;
  int expected_value_;
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
  base::RunLoop run_loop;
  constexpr int kFakePropertyValue = 1234;

  // Set up and create a dummy PageNode.
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  auto quit_closure = run_loop.QuitClosure();
  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(node);
    FakePageNodeDecoratorData::GetOrCreate(PageNodeImpl::FromNode(node.get()))
        ->SetOnSetPropertyCalledExpectations(std::move(quit_closure),
                                             kFakePropertyValue);
  });
  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);

  // Call to the tested function with SetProperty passed in as argument.
  // SetProperty contains the RunLoop's quit closure.
  SetPropertyForWebContentsPageNode(web_contents(),
                                    &FakePageNodeDecoratorData::SetProperty,
                                    kFakePropertyValue);

  // This will run until SetProperty calls the closure.
  run_loop.Run();
}

}  // namespace performance_manager
