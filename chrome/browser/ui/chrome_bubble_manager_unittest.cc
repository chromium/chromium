// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_bubble_manager.h"

#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_manager_mocks.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromeBubbleManagerTest : public BrowserWithTestWindowTest {
 public:
  ChromeBubbleManagerTest() {}

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<ChromeBubbleManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBubbleManagerTest);
};

void ChromeBubbleManagerTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  manager_ =
      std::make_unique<ChromeBubbleManager>(browser()->tab_strip_model());
}

void ChromeBubbleManagerTest::TearDown() {
  manager_.reset();
  BrowserWithTestWindowTest::TearDown();
}

TEST_F(ChromeBubbleManagerTest, CloseMockBubbleOnDestroy) {
  BubbleReference bubble1 = manager_->ShowBubble(MockBubbleDelegate::Default());
  manager_.reset();
  ASSERT_FALSE(bubble1);
}

TEST_F(ChromeBubbleManagerTest, CloseMockBubbleForTwoDifferentReasons) {
  BubbleReference bubble1 = manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference bubble2 = manager_->ShowBubble(MockBubbleDelegate::Default());

  bubble1->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
  bubble2->CloseBubble(BUBBLE_CLOSE_CANCELED);

  ASSERT_FALSE(bubble1);
  ASSERT_FALSE(bubble2);
}

TEST_F(ChromeBubbleManagerTest, CloseMockBubbleOnNavigate) {
  AddTab(browser(), GURL("https://foo/0"));

  std::unique_ptr<MockBubbleDelegate> delegate(new MockBubbleDelegate);
  EXPECT_CALL(*delegate, ShouldClose(BUBBLE_CLOSE_NAVIGATED))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate, DidClose(BUBBLE_CLOSE_NAVIGATED));
  EXPECT_CALL(*delegate, Destroyed());

  BubbleReference bubble_ref = manager_->ShowBubble(std::move(delegate));

  NavigateAndCommitActiveTab(GURL("https://foo/1"));

  ASSERT_FALSE(bubble_ref);
}

TEST_F(ChromeBubbleManagerTest, DontCloseBubbleWhenNavigationIsInPage) {
  AddTab(browser(), GURL("https://foo/0"));

  std::unique_ptr<MockBubbleDelegate> delegate(new MockBubbleDelegate);
  BubbleReference bubble_ref = manager_->ShowBubble(std::move(delegate));

  NavigateAndCommitActiveTab(GURL("https://foo/0#0"));

  ASSERT_TRUE(bubble_ref)
      << "The bubble shouldn't be destroyed when it is an in-page navigation.";
}

TEST_F(ChromeBubbleManagerTest, CloseMockBubbleOnOwningFrameDestroy) {
  AddTab(browser(), GURL("https://foo/0"));

  content::RenderFrameHostTester* main_frame =
      content::RenderFrameHostTester::For(
          browser()->tab_strip_model()->GetWebContentsAt(0)->GetMainFrame());

  content::RenderFrameHost* subframe0 = main_frame->AppendChild("subframe0");
  content::RenderFrameHostTester* subframe0_tester =
      content::RenderFrameHostTester::For(subframe0);

  content::RenderFrameHost* subframe1 = main_frame->AppendChild("subframe1");
  content::RenderFrameHostTester* subframe1_tester =
      content::RenderFrameHostTester::For(subframe1);

  std::unique_ptr<MockBubbleDelegate> delegate(new MockBubbleDelegate);
  EXPECT_CALL(*delegate, OwningFrame())
      .WillRepeatedly(testing::Return(subframe0));
  EXPECT_CALL(*delegate, ShouldClose(BUBBLE_CLOSE_FRAME_DESTROYED))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate, DidClose(BUBBLE_CLOSE_FRAME_DESTROYED));
  EXPECT_CALL(*delegate, Destroyed());

  BubbleReference bubble_ref = manager_->ShowBubble(std::move(delegate));

  subframe1_tester->Detach();
  EXPECT_TRUE(bubble_ref) << "The bubble shouldn't be destroyed when a "
                             "non-owning frame is destroyed.";

  subframe0_tester->Detach();
  EXPECT_FALSE(bubble_ref);
}

}  // namespace
