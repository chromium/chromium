// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "components/performance_manager/performance_manager_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// Helper function that allows testing that a PageLiveStateDecorator::Data
// property has the expected value. This function should be called from the main
// thread and be passed the WebContents pointer associated with the PageNode to
// check.
template <typename T>
void TestPropertyOnPMSequence(content::WebContents* contents,
                              T (PageLiveStateDecorator::Data::*getter)() const,
                              T expected_value) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPageNodeForWebContents(contents);

  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&](Graph* unused) {
        EXPECT_TRUE(node);
        auto* data =
            PageLiveStateDecorator::Data::GetOrCreateForTesting(node.get());
        EXPECT_TRUE(data);
        EXPECT_EQ((data->*getter)(), expected_value);
        std::move(quit_closure).Run();
      }));
  run_loop.Run();
}

}  // namespace

class PageLiveStateDecoratorTest : public PerformanceManagerTestHarness {
 protected:
  PageLiveStateDecoratorTest() = default;
  ~PageLiveStateDecoratorTest() override = default;
  PageLiveStateDecoratorTest(const PageLiveStateDecoratorTest& other) = delete;
  PageLiveStateDecoratorTest& operator=(const PageLiveStateDecoratorTest&) =
      delete;
};

TEST_F(PageLiveStateDecoratorTest, OnWebContentsAttachedToUSBChange) {
  auto contents = CreateTestWebContents();
  auto* contents_raw = contents.get();
  SetContents(std::move(contents));

  // By default the page shouldn't be connected to a USB device.
  TestPropertyOnPMSequence(
      contents_raw, &PageLiveStateDecorator::Data::IsAttachedToUSB, false);

  // Pretend that it attached to a USB device and make sure that the PageNode
  // data get updated.
  PageLiveStateDecorator::OnWebContentsAttachedToUSBChange(contents_raw, true);
  TestPropertyOnPMSequence(
      contents_raw, &PageLiveStateDecorator::Data::IsAttachedToUSB, true);

  // Switch back to the default state.
  PageLiveStateDecorator::OnWebContentsAttachedToUSBChange(contents_raw, false);
  TestPropertyOnPMSequence(
      contents_raw, &PageLiveStateDecorator::Data::IsAttachedToUSB, false);

  DeleteContents();
}

}  // namespace performance_manager
