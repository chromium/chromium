// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bubble/bubble_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_manager_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A "show chain" happens when a bubble decides to show another bubble on close.
// All bubbles must be iterated to handle a close event. If a bubble shows
// another bubble while it's being closed, the iterator can get messed up.
class ChainShowBubbleDelegate : public MockBubbleDelegate {
 public:
  // |chained_bubble| can be nullptr if not interested in getting a reference to
  // the chained bubble.
  ChainShowBubbleDelegate(BubbleManager* manager,
                          std::unique_ptr<BubbleDelegate> delegate,
                          BubbleReference* chained_bubble)
      : manager_(manager),
        delegate_(std::move(delegate)),
        chained_bubble_(chained_bubble),
        closed_(false) {
    EXPECT_CALL(*this, ShouldClose(testing::_)).WillOnce(testing::Return(true));
  }

  ~ChainShowBubbleDelegate() override { EXPECT_TRUE(closed_); }

  void DidClose(BubbleCloseReason reason) override {
    MockBubbleDelegate::DidClose(reason);
    BubbleReference ref = manager_->ShowBubble(std::move(delegate_));
    if (chained_bubble_)
      *chained_bubble_ = ref;
    closed_ = true;
  }

 private:
  BubbleManager* manager_;
  std::unique_ptr<BubbleDelegate> delegate_;
  BubbleReference* chained_bubble_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(ChainShowBubbleDelegate);
};

// A "close chain" happens when a close event is received while another close
// event is in progress. Ex: Closing the BubbleUi will hide the bubble, causing
// it to lose focus, which causes another close event. This test simulates this
// by sending a close event during the |DidClose| method of a BubbleDelegate.
// Similarly to the show chain, a close chain can mess up the iterator.
class ChainCloseBubbleDelegate : public MockBubbleDelegate {
 public:
  ChainCloseBubbleDelegate(BubbleManager* manager) : manager_(manager) {}

  ~ChainCloseBubbleDelegate() override {}

  void DidClose(BubbleCloseReason reason) override {
    manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);
  }

 private:
  BubbleManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ChainCloseBubbleDelegate);
};

class MockBubbleManagerObserver : public BubbleManager::BubbleManagerObserver {
 public:
  MockBubbleManagerObserver() {}
  ~MockBubbleManagerObserver() override {}

  MOCK_METHOD1(OnBubbleNeverShown, void(BubbleReference));
  MOCK_METHOD2(OnBubbleClosed, void(BubbleReference, BubbleCloseReason));
  MOCK_METHOD1(OnBubbleShown, void(BubbleReference));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBubbleManagerObserver);
};

class BubbleManagerSubclass : public BubbleManager {
 public:
  using BubbleManager::CloseBubblesOwnedBy;
};

class BubbleManagerTest : public testing::Test {
 public:
  BubbleManagerTest();
  ~BubbleManagerTest() override {}

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<BubbleManagerSubclass> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleManagerTest);
};

BubbleManagerTest::BubbleManagerTest() {}

void BubbleManagerTest::SetUp() {
  testing::Test::SetUp();
  manager_.reset(new BubbleManagerSubclass);
}

void BubbleManagerTest::TearDown() {
  manager_.reset();
  testing::Test::TearDown();
}

TEST_F(BubbleManagerTest, ManagerShowsBubbleUi) {
  std::unique_ptr<MockBubbleDelegate> delegate = MockBubbleDelegate::Default();

  MockBubbleUi* bubble_ui = delegate->bubble_ui();
  EXPECT_CALL(*bubble_ui, Destroyed());
  EXPECT_CALL(*bubble_ui, Show(testing::_));
  EXPECT_CALL(*bubble_ui, Close());
  EXPECT_CALL(*bubble_ui, UpdateAnchorPosition()).Times(0);

  manager_->ShowBubble(std::move(delegate));
}

TEST_F(BubbleManagerTest, ManagerUpdatesBubbleUiAnchor) {
  std::unique_ptr<MockBubbleDelegate> delegate = MockBubbleDelegate::Default();

  MockBubbleUi* bubble_ui = delegate->bubble_ui();
  EXPECT_CALL(*bubble_ui, Destroyed());
  EXPECT_CALL(*bubble_ui, Show(testing::_));
  EXPECT_CALL(*bubble_ui, Close());
  EXPECT_CALL(*bubble_ui, UpdateAnchorPosition());

  manager_->ShowBubble(std::move(delegate));
  manager_->UpdateAllBubbleAnchors();
}

TEST_F(BubbleManagerTest, CloseOnReferenceInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  ASSERT_TRUE(ref->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseOnStubbornReferenceDoesNotInvalidate) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_FALSE(ref->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, DestroyInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_.reset();

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FORCED);

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, DestroyInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_.reset();

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseDoesNotInvalidateStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_FALSE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseAllDoesNotInvalidateStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesMixAppropriately) {
  BubbleReference stubborn_ref1 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref1 =
      manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference stubborn_ref2 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref2 =
      manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference stubborn_ref3 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref3 =
      manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_TRUE(stubborn_ref1);
  ASSERT_TRUE(stubborn_ref2);
  ASSERT_TRUE(stubborn_ref3);
  ASSERT_FALSE(normal_ref1);
  ASSERT_FALSE(normal_ref2);
  ASSERT_FALSE(normal_ref3);
}

TEST_F(BubbleManagerTest, CloseBubbleShouldOnlylCloseSelf) {
  BubbleReference ref1 = manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference ref2 = manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference ref3 = manager_->ShowBubble(MockBubbleDelegate::Default());

  EXPECT_TRUE(ref1);
  EXPECT_TRUE(ref2);
  EXPECT_TRUE(ref3);

  ref2->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST);

  EXPECT_TRUE(ref1);
  EXPECT_FALSE(ref2);
  EXPECT_TRUE(ref3);
}

TEST_F(BubbleManagerTest, CloseOwnedByShouldLeaveUnowned) {
  std::unique_ptr<MockBubbleDelegate> delegate1 = MockBubbleDelegate::Default();
  std::unique_ptr<MockBubbleDelegate> delegate2 = MockBubbleDelegate::Default();
  std::unique_ptr<MockBubbleDelegate> delegate3 = MockBubbleDelegate::Default();
  MockBubbleDelegate& delegate1_ref = *delegate1;
  MockBubbleDelegate& delegate2_ref = *delegate2;
  MockBubbleDelegate& delegate3_ref = *delegate3;
  BubbleReference ref1 = manager_->ShowBubble(std::move(delegate1));
  BubbleReference ref2 = manager_->ShowBubble(std::move(delegate2));
  BubbleReference ref3 = manager_->ShowBubble(std::move(delegate3));

  // These pointers are only compared for equality, not dereferenced.
  const content::RenderFrameHost* const frame1 =
      reinterpret_cast<const content::RenderFrameHost*>(&ref1);
  const content::RenderFrameHost* const frame2 =
      reinterpret_cast<const content::RenderFrameHost*>(&ref2);

  EXPECT_CALL(delegate1_ref, OwningFrame())
      .WillRepeatedly(testing::Return(frame1));
  EXPECT_CALL(delegate2_ref, OwningFrame())
      .WillRepeatedly(testing::Return(frame2));
  EXPECT_CALL(delegate3_ref, OwningFrame())
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_CALL(delegate1_ref, ShouldClose(BUBBLE_CLOSE_FRAME_DESTROYED))
      .WillOnce(testing::Return(true));

  manager_->CloseBubblesOwnedBy(frame1);
  EXPECT_FALSE(ref1);
  EXPECT_TRUE(ref2);
  EXPECT_TRUE(ref3);
}

TEST_F(BubbleManagerTest, UpdateAllShouldWorkWithoutBubbles) {
  // Manager shouldn't crash if bubbles have never been added.
  manager_->UpdateAllBubbleAnchors();

  // Add a bubble and close it.
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  // Bubble should NOT get an update event because it's already closed.
  manager_->UpdateAllBubbleAnchors();
}

TEST_F(BubbleManagerTest, CloseAllShouldWorkWithoutBubbles) {
  // Manager shouldn't crash if bubbles have never been added.
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  // Add a bubble and close it.
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  // Bubble should NOT get a close event because it's already closed.
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);
}

// This test validates that it's possible to show another bubble when
// |CloseBubble| is called.
TEST_F(BubbleManagerTest, AllowBubbleChainingOnClose) {
  BubbleReference chained_bubble;
  BubbleReference ref =
      manager_->ShowBubble(std::make_unique<ChainShowBubbleDelegate>(
          manager_.get(), MockBubbleDelegate::Default(), &chained_bubble));
  ASSERT_FALSE(chained_bubble);  // Bubble not yet visible.
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));
  ASSERT_TRUE(chained_bubble);  // Bubble is now visible.
}

// This test validates that it's possible to show another bubble when
// |CloseAllBubbles| is called.
TEST_F(BubbleManagerTest, AllowBubbleChainingOnCloseAll) {
  BubbleReference chained_bubble;
  BubbleReference ref =
      manager_->ShowBubble(std::make_unique<ChainShowBubbleDelegate>(
          manager_.get(), MockBubbleDelegate::Default(), &chained_bubble));
  ASSERT_FALSE(chained_bubble);  // Bubble not yet visible.
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FORCED);
  ASSERT_TRUE(chained_bubble);  // Bubble is now visible.
}

// This test validates that a show chain will not happen in the destructor.
// While chaining is during the normal life span of the manager, it should NOT
// happen when the manager is being destroyed.
TEST_F(BubbleManagerTest, BubblesDoNotChainOnDestroy) {
  MockBubbleManagerObserver metrics;
  // |chained_delegate| should never be shown.
  EXPECT_CALL(metrics, OnBubbleNeverShown(testing::_));
  // The ChainShowBubbleDelegate should be closed when the manager is destroyed.
  EXPECT_CALL(metrics, OnBubbleClosed(testing::_, BUBBLE_CLOSE_FORCED));
  manager_->AddBubbleManagerObserver(&metrics);

  std::unique_ptr<MockBubbleDelegate> chained_delegate(new MockBubbleDelegate);
  EXPECT_CALL(*chained_delegate->bubble_ui(), Show(testing::_)).Times(0);
  EXPECT_CALL(*chained_delegate, ShouldClose(testing::_)).Times(0);
  EXPECT_CALL(*chained_delegate, DidClose(testing::_)).Times(0);

  manager_->ShowBubble(std::make_unique<ChainShowBubbleDelegate>(
      manager_.get(), std::move(chained_delegate), nullptr));
  manager_.reset();
}

TEST_F(BubbleManagerTest, BubbleCloseReasonIsCalled) {
  MockBubbleManagerObserver metrics;
  EXPECT_CALL(metrics, OnBubbleNeverShown(testing::_)).Times(0);
  EXPECT_CALL(metrics, OnBubbleClosed(testing::_, BUBBLE_CLOSE_ACCEPTED));
  manager_->AddBubbleManagerObserver(&metrics);

  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ref->CloseBubble(BUBBLE_CLOSE_ACCEPTED);

  // Destroy to verify no events are sent to |metrics| in destructor.
  manager_.reset();
}

TEST_F(BubbleManagerTest, OnBubbleShownIsCalled) {
  MockBubbleManagerObserver metrics;
  EXPECT_CALL(metrics, OnBubbleNeverShown(testing::_)).Times(0);
  EXPECT_CALL(metrics, OnBubbleShown(testing::_));
  manager_->AddBubbleManagerObserver(&metrics);

  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ref->CloseBubble(BUBBLE_CLOSE_ACCEPTED);

  // Destroy to verify no events are sent to |metrics| in destructor.
  manager_.reset();
}

// In a close chain, it should be possible for the bubble in the second close
// event to close.
TEST_F(BubbleManagerTest, BubbleCloseChainCloseClose) {
  std::unique_ptr<ChainCloseBubbleDelegate> closing_bubble(
      new ChainCloseBubbleDelegate(manager_.get()));
  EXPECT_CALL(*closing_bubble, ShouldClose(testing::_))
      .WillOnce(testing::Return(true));

  BubbleReference other_bubble_ref =
      manager_->ShowBubble(MockBubbleDelegate::Default());

  BubbleReference closing_bubble_ref =
      manager_->ShowBubble(std::move(closing_bubble));

  EXPECT_TRUE(other_bubble_ref);
  EXPECT_TRUE(closing_bubble_ref);

  closing_bubble_ref->CloseBubble(BUBBLE_CLOSE_ACCEPTED);

  EXPECT_FALSE(other_bubble_ref);
  EXPECT_FALSE(closing_bubble_ref);
}

// In a close chain, it should be possible for the bubble in the second close
// event to remain open because close is a request.
TEST_F(BubbleManagerTest, BubbleCloseChainCloseNoClose) {
  std::unique_ptr<ChainCloseBubbleDelegate> closing_bubble(
      new ChainCloseBubbleDelegate(manager_.get()));
  EXPECT_CALL(*closing_bubble, ShouldClose(testing::_))
      .WillOnce(testing::Return(true));

  BubbleReference other_bubble_ref =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  BubbleReference closing_bubble_ref =
      manager_->ShowBubble(std::move(closing_bubble));

  EXPECT_TRUE(other_bubble_ref);
  EXPECT_TRUE(closing_bubble_ref);

  closing_bubble_ref->CloseBubble(BUBBLE_CLOSE_ACCEPTED);

  EXPECT_TRUE(other_bubble_ref);
  EXPECT_FALSE(closing_bubble_ref);
}

// This test is a sanity check. |closing_bubble| will attempt to close all other
// bubbles if it's closed, but it doesn't want to close. Sending a close request
// should keep it open without starting a close chain.
TEST_F(BubbleManagerTest, BubbleCloseChainNoCloseNoClose) {
  std::unique_ptr<ChainCloseBubbleDelegate> closing_bubble(
      new ChainCloseBubbleDelegate(manager_.get()));
  EXPECT_CALL(*closing_bubble, ShouldClose(testing::_))
      .WillRepeatedly(testing::Return(false));

  BubbleReference other_bubble_ref =
      manager_->ShowBubble(MockBubbleDelegate::Default());

  BubbleReference closing_bubble_ref =
      manager_->ShowBubble(std::move(closing_bubble));

  EXPECT_TRUE(other_bubble_ref);
  EXPECT_TRUE(closing_bubble_ref);

  closing_bubble_ref->CloseBubble(BUBBLE_CLOSE_ACCEPTED);

  EXPECT_TRUE(other_bubble_ref);
  EXPECT_TRUE(closing_bubble_ref);
}

}  // namespace
