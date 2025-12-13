// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/bubble_manager_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  ~FakeTabInterface() override = default;
  explicit FakeTabInterface(TestingProfile* testing_profile);
  base::CallbackListSubscription RegisterDidActivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override;
  base::CallbackListSubscription RegisterWillDeactivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override;
  content::WebContents* GetContents() const override { return web_contents_; }
  void Activate();
  void Deactivate();
  bool IsActivated() const override { return is_activated_; }

 private:
  // Only created if a non-null profile is provided.
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  // Owned by `web_contents_factory_`.
  raw_ptr<content::WebContents> web_contents_;
  bool is_activated_ = false;
  base::RepeatingCallbackList<void(TabInterface*)> activation_callbacks_;
  base::RepeatingCallbackList<void(TabInterface*)> deactivation_callbacks_;
};

FakeTabInterface::FakeTabInterface(TestingProfile* testing_profile) {
  if (testing_profile) {
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    web_contents_ = web_contents_factory_->CreateWebContents(testing_profile);
  }
}

base::CallbackListSubscription FakeTabInterface::RegisterDidActivate(
    base::RepeatingCallback<void(TabInterface*)> cb) {
  return activation_callbacks_.Add(cb);
}

base::CallbackListSubscription FakeTabInterface::RegisterWillDeactivate(
    base::RepeatingCallback<void(TabInterface*)> cb) {
  return deactivation_callbacks_.Add(cb);
}

void FakeTabInterface::Activate() {
  is_activated_ = true;
  activation_callbacks_.Notify(this);
}

void FakeTabInterface::Deactivate() {
  is_activated_ = false;
  deactivation_callbacks_.Notify(this);
}

// A mock implementation of BubbleControllerBase to use in tests.
class MockBubbleController : public BubbleControllerBase {
 public:
  MockBubbleController() {
    // Default behavior for mock methods.
    ON_CALL(*this, ShowBubble).WillByDefault([this]() { is_shown_ = true; });
    ON_CALL(*this, HideBubble).WillByDefault([this]() { is_shown_ = false; });
    ON_CALL(*this, IsShowingBubble).WillByDefault([this]() {
      return is_shown_;
    });
    ON_CALL(*this, GetBubbleControllerBaseWeakPtr).WillByDefault([this]() {
      return weak_ptr_factory_.GetWeakPtr();
    });
  }

  MOCK_METHOD(void, ShowBubble, (), (override));
  MOCK_METHOD(void, HideBubble, (bool), (override));
  MOCK_METHOD(void, OnBubbleDiscarded, (), (override));
  MOCK_METHOD(bool, CanBeReshown, (), (const, override));
  MOCK_METHOD(BubbleType, GetBubbleType, (), (const, override));
  MOCK_METHOD(bool, IsShowingBubble, (), (const, override));
  MOCK_METHOD(bool, IsMouseHovered, (), (const, override));
  MOCK_METHOD(base::WeakPtr<BubbleControllerBase>,
              GetBubbleControllerBaseWeakPtr,
              (),
              (override));

  void SetBubbleType(BubbleType type) {
    ON_CALL(*this, GetBubbleType).WillByDefault(Return(type));
  }

 private:
  bool is_shown_ = false;
  base::WeakPtrFactory<BubbleControllerBase> weak_ptr_factory_{this};
};

class BubbleManagerImplTest : public ::testing::Test {
 public:
  BubbleManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // Creates and returns a new `MockBubbleController` with `bubble_type`.
  std::unique_ptr<MockBubbleController> CreateController(
      BubbleType bubble_type,
      bool can_be_reshown = true) {
    std::unique_ptr<MockBubbleController> controller =
        std::make_unique<MockBubbleController>();
    controller->SetBubbleType(bubble_type);
    ON_CALL(*controller, CanBeReshown).WillByDefault(Return(can_be_reshown));
    return controller;
  }

 protected:
  void SetUp() override {
    Test::SetUp();

    tab_interface_ = std::make_unique<FakeTabInterface>(&profile_);
    bubble_manager_ = std::make_unique<BubbleManagerImpl>(tab_interface_.get());
    tab_interface_->Activate();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  BubbleManagerImpl& bubble_manager() const { return *bubble_manager_; }

  FakeTabInterface* tab_interface() const { return tab_interface_.get(); }

  TestingProfile* profile() { return &profile_; }

  void ResetBubbleManager() { bubble_manager_.reset(); }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
  std::unique_ptr<BubbleManagerImpl> bubble_manager_;
};

// Test that requesting a bubble when none is active shows it immediately.
TEST_F(BubbleManagerImplTest, RequestShow_NoActiveBubble_ShowsImmediately) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  EXPECT_CALL(*address_controller, ShowBubble());
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  EXPECT_TRUE(address_controller->IsShowingBubble());

  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.RequestShow",
                                       BubbleType::kSaveUpdateAddress, 1);
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Show.NoActiveBubble",
                                       BubbleType::kSaveUpdateAddress, 1);
  histogram_tester_.ExpectTotalCount("Autofill.Bubble.RequestShow.ForceShow",
                                     0);
}

// Test that a higher-priority bubble preempts a lower-priority one.
TEST_F(BubbleManagerImplTest, RequestShow_HigherPriority_PreemptsActive) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  EXPECT_CALL(*address_controller, ShowBubble());
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(address_controller->IsShowingBubble());

  {
    InSequence sequence;
    EXPECT_CALL(*address_controller,
                HideBubble(/*initiated_by_bubble_manager=*/true));
    EXPECT_CALL(*card_controller, ShowBubble());
  }

  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  EXPECT_FALSE(address_controller->IsShowingBubble());
  EXPECT_TRUE(card_controller->IsShowingBubble());

  histogram_tester_.ExpectBucketCount("Autofill.Bubble.RequestShow",
                                      BubbleType::kSaveUpdateAddress, 1);
  histogram_tester_.ExpectBucketCount("Autofill.Bubble.RequestShow",
                                      BubbleType::kSaveUpdateCard, 1);
  histogram_tester_.ExpectTotalCount("Autofill.Bubble.RequestShow", 2);
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Show.NoActiveBubble",
                                       BubbleType::kSaveUpdateAddress, 1);
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Show.Preemption",
                                       BubbleType::kSaveUpdateCard, 1);
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.WasPreempted",
                                       BubbleType::kSaveUpdateAddress, 1);
}

// Test that hiding the active bubble shows the next highest-priority one from
// the queue.
TEST_F(BubbleManagerImplTest, HideActiveBubble_WithPendingRequest_ShowsNext) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  // Show card bubble, then queue address bubble.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_FALSE(address_controller->IsShowingBubble());

  // When the active (card) bubble is hidden, the address bubble should be shown
  // from the queue.
  EXPECT_CALL(*address_controller, ShowBubble());

  // Hide the card bubble.
  bubble_manager().OnBubbleHiddenByController(*card_controller,
                                              /*show_next_bubble=*/true);

  // The state of the card controller should now be false.
  card_controller->HideBubble(/*initiated_by_bubble_manager=*/true);

  EXPECT_FALSE(card_controller->IsShowingBubble());
  EXPECT_TRUE(address_controller->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Queue.ShownFromQueue",
                                       BubbleType::kSaveUpdateAddress, 1);
}

// Test that hiding a bubble with `show_next_bubble` set to false does not
// trigger the next bubble in the queue.
TEST_F(BubbleManagerImplTest, HideActiveBubble_DoNotShowNextBubble) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  // Show the high-priority card bubble.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  // Queue the lower-priority address bubble. It should not be shown.
  EXPECT_CALL(*address_controller, ShowBubble()).Times(0);
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_FALSE(address_controller->IsShowingBubble());

  // Hide the card bubble, instructing the manager NOT to show the next one.
  // The address bubble should not be shown.
  bubble_manager().OnBubbleHiddenByController(*card_controller,
                                              /*show_next_bubble=*/false);

  // Manually update the hidden bubble's state.
  card_controller->HideBubble(/*initiated_by_bubble_manager=*/true);

  EXPECT_FALSE(card_controller->IsShowingBubble());
  EXPECT_FALSE(address_controller->IsShowingBubble());
}

// Tests that when a high priority bubble is shown, the lower priority bubbles
// from the queue do not get shown.
TEST_F(BubbleManagerImplTest,
       HideBubbleDuringPreemption_DoesNotShowNextFromQueue) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);

  // Show card bubble and then queue address bubble.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_FALSE(address_controller->IsShowingBubble());

  // Request a high-priority password bubble. This will preempt the active
  // card bubble.
  {
    InSequence sequence;
    EXPECT_CALL(*card_controller,
                HideBubble(/*initiated_by_bubble_manager=*/true));
    EXPECT_CALL(*password_controller, ShowBubble());
  }

  // Ensure that the queued address bubble is never shown during this process.
  EXPECT_CALL(*address_controller, ShowBubble()).Times(0);
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);

  // The password bubble is now active, and the other two are not.
  EXPECT_TRUE(password_controller->IsShowingBubble());
  EXPECT_FALSE(address_controller->IsShowingBubble());
  EXPECT_FALSE(card_controller->IsShowingBubble());
}

// Test that a lower-priority bubble is queued if a higher-priority one is
// active.
TEST_F(BubbleManagerImplTest, RequestShow_LowerPriority_QueuesRequest) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  EXPECT_CALL(*address_controller, ShowBubble()).Times(0);
  EXPECT_CALL(*card_controller,
              HideBubble(/*initiated_by_bubble_manager=*/true))
      .Times(0);
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);

  EXPECT_TRUE(card_controller->IsShowingBubble());
  EXPECT_FALSE(address_controller->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bubble.Queue.AddedDueToActiveBubble",
      BubbleType::kSaveUpdateAddress, 1);
}

// Test that a bubble with a preempt-same-type policy replaces an existing one.
TEST_F(BubbleManagerImplTest,
       RequestShow_SameTypeWithPreemptPolicy_PreemptsActive) {
  std::unique_ptr<MockBubbleController> password_controller_1 =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> password_controller_2 =
      CreateController(BubbleType::kPassword);

  // Show first password bubble and then replace it.
  EXPECT_CALL(*password_controller_1, ShowBubble());
  bubble_manager().RequestShowController(*password_controller_1,
                                         /*force_show=*/false);
  ASSERT_TRUE(password_controller_1->IsShowingBubble());

  {
    InSequence sequence;
    EXPECT_CALL(*password_controller_1,
                HideBubble(/*initiated_by_bubble_manager=*/true));
    EXPECT_CALL(*password_controller_2, ShowBubble());
  }

  bubble_manager().RequestShowController(*password_controller_2,
                                         /*force_show=*/false);
  EXPECT_FALSE(password_controller_1->IsShowingBubble());
  EXPECT_TRUE(password_controller_2->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.WasPreempted",
                                       BubbleType::kPassword, 1);
}

// Test that a pending request is ignored if a request of the same type
// exists and the timeout has not been reached for bubbles without the preempt
// policy.
TEST_F(BubbleManagerImplTest, AddToQueue_DuplicateType_IgnoredBeforeTimeout) {
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> address_controller_1 =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> address_controller_2 =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show password bubbles and couple of address bubbles back-to-back.
  EXPECT_CALL(*password_controller, ShowBubble());
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);
  bubble_manager().RequestShowController(*address_controller_1,
                                         /*force_show=*/false);
  bubble_manager().RequestShowController(*address_controller_2,
                                         /*force_show=*/false);

  bubble_manager().OnBubbleHiddenByController(*password_controller,
                                              /*show_next_bubble=*/true);

  EXPECT_TRUE(address_controller_1->IsShowingBubble());
  EXPECT_FALSE(address_controller_2->IsShowingBubble());

  // Ensure `address_controller_2` is never shown.
  bubble_manager().OnBubbleHiddenByController(*address_controller_1,
                                              /*show_next_bubble=*/true);
  EXPECT_FALSE(address_controller_2->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Queue.Discarded",
                                       BubbleType::kSaveUpdateAddress, 1);
}

// Test that a pending request replaces an older one of the same type after
// timeout for bubbles without the preempt policy.
TEST_F(BubbleManagerImplTest, AddToQueue_DuplicateType_ReplacedAfterTimeout) {
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> address_controller_1 =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> address_controller_2 =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show password bubbles and address bubbles. Then add another address bubble
  // after 3601 seconds.
  EXPECT_CALL(*password_controller, ShowBubble());
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);
  bubble_manager().RequestShowController(*address_controller_1,
                                         /*force_show=*/false);

  FastForwardBy(base::Seconds(3601));

  // When adding `address_controller_2`, the manager should see that
  // `address_controller_1` has timed out and call the new method.
  EXPECT_CALL(*address_controller_1, OnBubbleDiscarded());
  bubble_manager().RequestShowController(*address_controller_2,
                                         /*force_show=*/false);

  bubble_manager().OnBubbleHiddenByController(*password_controller,
                                              /*show_next_bubble=*/true);

  EXPECT_FALSE(address_controller_1->IsShowingBubble());
  EXPECT_TRUE(address_controller_2->IsShowingBubble());

  // Ensure `address_controller_1` is never shown.
  bubble_manager().OnBubbleHiddenByController(*address_controller_2,
                                              /*show_next_bubble=*/true);
  EXPECT_FALSE(address_controller_1->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Queue.TimedOut",
                                       BubbleType::kSaveUpdateAddress, 1);
}

// Test that a new bubble with a preempt policy always replaces a pending one.
TEST_F(BubbleManagerImplTest,
       AddToQueue_DuplicateTypeWithPreemptPolicy_ReplacesImmediately) {
  std::unique_ptr<MockBubbleController> password_controller_1 =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> password_controller_2 =
      CreateController(BubbleType::kPassword);
  // It has higher priority than password bubbles.
  std::unique_ptr<MockBubbleController> filled_card_controller =
      CreateController(BubbleType::kFilledCardInformation);

  // Filled card controller is shown and then password controllers are added to
  // the queue.
  EXPECT_CALL(*filled_card_controller, ShowBubble());
  bubble_manager().RequestShowController(*filled_card_controller,
                                         /*force_show=*/false);
  bubble_manager().RequestShowController(*password_controller_1,
                                         /*force_show=*/false);
  bubble_manager().RequestShowController(*password_controller_2,
                                         /*force_show=*/false);

  bubble_manager().OnBubbleHiddenByController(*filled_card_controller,
                                              /*show_next_bubble=*/true);
  EXPECT_FALSE(password_controller_1->IsShowingBubble());
  EXPECT_TRUE(password_controller_2->IsShowingBubble());

  // Ensure `password_controller_1` is never shown.
  bubble_manager().OnBubbleHiddenByController(*password_controller_2,
                                              /*show_next_bubble=*/true);
  EXPECT_FALSE(password_controller_1->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Queue.Replaced",
                                       BubbleType::kPassword, 1);
}

// Test that a higher-priority bubble does NOT preempt a lower-priority one if
// the mouse is hovered.
TEST_F(BubbleManagerImplTest,
       RequestShow_HigherPriority_DoesNotPreemptIfHovered) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  EXPECT_CALL(*address_controller, ShowBubble());
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(address_controller->IsShowingBubble());

  // Simulate mouse hover.
  ON_CALL(*address_controller, IsMouseHovered).WillByDefault(Return(true));

  // Card bubble should not be shown, address bubble should not be hidden.
  EXPECT_CALL(*card_controller, ShowBubble()).Times(0);
  EXPECT_CALL(*address_controller,
              HideBubble(/*initiated_by_bubble_manager=*/true))
      .Times(0);
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);

  EXPECT_TRUE(address_controller->IsShowingBubble());
  EXPECT_FALSE(card_controller->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Queue.AddedDueToHover",
                                       BubbleType::kSaveUpdateCard, 1);
}

// Test that HasPendingBubble returns false when no bubble is pending.
TEST_F(BubbleManagerImplTest, HasPendingBubble_NoBubble_ReturnsFalse) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      address_controller->GetBubbleType()));
}

// Test that HasPendingBubble returns true for a valid, non-expired pending
// bubble.
TEST_F(BubbleManagerImplTest, HasPendingBubble_BubblePending_ReturnsTrue) {
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  // Show a high-priority bubble to ensure the next one is queued.
  EXPECT_CALL(*password_controller, ShowBubble());
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);

  // Queue the address bubble.
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);

  // Check that the address bubble is correctly reported as pending.
  EXPECT_TRUE(bubble_manager().HasConflictingPendingBubble(
      address_controller->GetBubbleType()));
  // Check that a bubble type not in the queue returns false.
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      card_controller->GetBubbleType()));
}

// Test that HasPendingBubble returns false for a timed-out bubble.
TEST_F(BubbleManagerImplTest,
       HasPendingBubble_BubblePending_TimedOut_ReturnsFalse) {
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show a high-priority bubble.
  EXPECT_CALL(*password_controller, ShowBubble());
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);

  // Queue the address bubble and confirm it's pending.
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(bubble_manager().HasConflictingPendingBubble(
      address_controller->GetBubbleType()));

  // Fast forward time past the timeout.
  FastForwardBy(base::Seconds(3601));

  // Now, checking for the bubble should return false because it has timed out.
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      address_controller->GetBubbleType()));
}

// Test that a bubble is timed out from the queue.
TEST_F(BubbleManagerImplTest, ProcessPendingBubbles_TimedOut) {
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show a high-priority bubble.
  EXPECT_CALL(*password_controller, ShowBubble());
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);

  // Queue the address bubble and confirm it's pending.
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(bubble_manager().HasConflictingPendingBubble(
      address_controller->GetBubbleType()));

  // Fast forward time past the timeout.
  FastForwardBy(base::Seconds(3601));

  // Hiding the current bubble will trigger processing the pending bubbles.
  // The address bubble should be timed out and not shown.
  EXPECT_CALL(*address_controller, ShowBubble()).Times(0);
  EXPECT_CALL(*address_controller, OnBubbleDiscarded());
  bubble_manager().OnBubbleHiddenByController(*password_controller,
                                              /*show_next_bubble=*/true);

  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.Queue.TimedOut",
                                       BubbleType::kSaveUpdateAddress, 1);
}

// Test that a force_show request preempts a higher-priority active bubble.
TEST_F(BubbleManagerImplTest, RequestShow_ForceShow_PreemptsActiveBubble) {
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show the high-priority bubble first.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  // Expect the active bubble to be hidden and the new one shown.
  {
    InSequence sequence;
    EXPECT_CALL(*card_controller,
                HideBubble(/*initiated_by_bubble_manager=*/true));
    EXPECT_CALL(*address_controller, ShowBubble());
  }

  // Force show the lower-priority bubble. It should preempt the active one.
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/true);
  EXPECT_FALSE(card_controller->IsShowingBubble());
  EXPECT_TRUE(address_controller->IsShowingBubble());

  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.RequestShow.ForceShow",
                                       BubbleType::kSaveUpdateAddress, 1);
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.WasPreempted",
                                       BubbleType::kSaveUpdateCard, 1);
}

// Test that the active bubble is hidden and queued when the tab visibility
// changes to hidden.
TEST_F(BubbleManagerImplTest, OnVisibilityChanged_Hidden_HidesAndQueuesBubble) {
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  // Show a bubble, making it active.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  // Expect the bubble to be hidden when visibility changes.
  EXPECT_CALL(*card_controller,
              HideBubble(/*initiated_by_bubble_manager=*/true));

  // Simulate the tab becoming hidden.
  tab_interface()->Deactivate();

  // The bubble should no longer be considered "showing".
  EXPECT_FALSE(card_controller->IsShowingBubble());

  // To verify it was queued, we can simulate the tab becoming visible again.
  EXPECT_CALL(*card_controller, ShowBubble());
  tab_interface()->Activate();
  EXPECT_TRUE(card_controller->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.HideDueToTabHide",
                                       BubbleType::kSaveUpdateCard, 1);
}

// Test that a pending bubble is shown when the tab visibility changes to
// visible.
TEST_F(BubbleManagerImplTest, OnVisibilityChanged_Visible_ShowsPendingBubble) {
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show a high-priority bubble, then queue a lower-priority one.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());
  ASSERT_FALSE(address_controller->IsShowingBubble());

  // Hide the active bubble and simulate the tab becoming hidden.
  // This leaves the address bubble in the queue.
  EXPECT_CALL(*card_controller,
              HideBubble(/*initiated_by_bubble_manager=*/true));
  tab_interface()->Deactivate();
  ASSERT_FALSE(card_controller->IsShowingBubble());

  // When the tab becomes visible again, the card bubble should show again as
  // it has higher priority.
  EXPECT_CALL(*card_controller, ShowBubble());
  tab_interface()->Activate();

  EXPECT_TRUE(card_controller->IsShowingBubble());
}

// Test that no action is taken when the tab becomes hidden and there is no
// active bubble.
TEST_F(BubbleManagerImplTest, OnVisibilityChanged_Hidden_NoActiveBubble) {
  // Ensure no mock calls are expected on any controllers.
  // We can create a controller but never show it.
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  EXPECT_CALL(*card_controller,
              HideBubble(/*initiated_by_bubble_manager=*/true))
      .Times(0);

  // Simulate the tab becoming hidden. The test will fail if any unexpected
  // methods are called.
  tab_interface()->Deactivate();
  histogram_tester_.ExpectTotalCount("Autofill.Bubble.HideDueToTabHide", 0);
}

// Test that `force_show` preempts an active bubble regardless of priority or
// hover state.
TEST_F(BubbleManagerImplTest,
       RequestShow_ForceShow_PreemptsRegardlessOfPriority) {
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show the higher-priority card bubble.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  // Simulate hovering to ensure `force_show` bypasses it.
  ON_CALL(*card_controller, IsMouseHovered).WillByDefault(Return(true));
  {
    InSequence sequence;
    EXPECT_CALL(*card_controller,
                HideBubble(/*initiated_by_bubble_manager=*/true));
    EXPECT_CALL(*address_controller, ShowBubble());
  }

  // Force show the lower-priority address bubble.
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/true);
  EXPECT_FALSE(card_controller->IsShowingBubble());
  EXPECT_TRUE(address_controller->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.RequestShow.ForceShow",
                                       BubbleType::kSaveUpdateAddress, 1);
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.WasPreempted",
                                       BubbleType::kSaveUpdateCard, 1);
}

// Test that the active bubble is hidden and queued when the tab is deactivated.
TEST_F(BubbleManagerImplTest, TabDeactivated_ActiveBubbleIsQueuedAndHidden) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  EXPECT_CALL(*address_controller, ShowBubble());
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(address_controller->IsShowingBubble());

  // Deactivating the tab should hide the bubble.
  EXPECT_CALL(*address_controller,
              HideBubble(/*initiated_by_bubble_manager=*/true));
  tab_interface()->Deactivate();
  EXPECT_FALSE(address_controller->IsShowingBubble());

  // When the tab is reactivated, the bubble should be shown again from the
  // queue.
  EXPECT_CALL(*address_controller, ShowBubble());
  tab_interface()->Activate();
  EXPECT_TRUE(address_controller->IsShowingBubble());
  histogram_tester_.ExpectUniqueSample("Autofill.Bubble.HideDueToTabHide",
                                       BubbleType::kSaveUpdateAddress, 1);
}

// Test that `ProcessPendingBubbles` cleans up stale pointers from the queue.
TEST_F(BubbleManagerImplTest, ProcessPendingBubbles_CleansUpStaleControllers) {
  auto card_controller = CreateController(BubbleType::kSaveUpdateCard);
  auto address_controller = CreateController(BubbleType::kSaveUpdateAddress);

  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);

  // Queue the address bubble.
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  EXPECT_TRUE(card_controller->IsShowingBubble());
  EXPECT_FALSE(address_controller->IsShowingBubble());

  // Destroy the address controller, invalidating its weak pointer.
  address_controller.reset();
  bubble_manager().OnBubbleHiddenByController(*card_controller,
                                              /*show_next_bubble=*/true);
}

// Test that the time a bubble spends in the queue is logged correctly.
TEST_F(BubbleManagerImplTest, HideActiveBubble_LogsTimeInQueue) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  // Show card bubble, then queue address bubble.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);

  // Advance time by 5 seconds.
  FastForwardBy(base::Seconds(5));

  // Hide the active bubble, which should trigger the queued bubble to show.
  EXPECT_CALL(*address_controller, ShowBubble());
  bubble_manager().OnBubbleHiddenByController(*card_controller,
                                              /*show_next_bubble=*/true);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.Bubble.Queue.TimeInQueue.SaveUpdateAddress", base::Seconds(5),
      1);
}

// Test that on bubble manager destruction, pending bubbles are timed out.
TEST_F(BubbleManagerImplTest,
       BubbleManagerDestruction_PendingBubbleIsTimedOut) {
  // Create a high-priority controller to occupy the active slot.
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show the high-priority bubble. It becomes the active one.
  EXPECT_CALL(*password_controller, ShowBubble());
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(password_controller->IsShowingBubble());

  // Now, request the address bubble. Because a higher-priority bubble is
  // active, this one will be placed in the pending queue.
  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  ASSERT_FALSE(address_controller->IsShowingBubble());
  ASSERT_TRUE(bubble_manager().HasConflictingPendingBubble(
      BubbleType::kSaveUpdateAddress));

  EXPECT_CALL(*address_controller, OnBubbleDiscarded());

  // Destroy the bubble manager.
  ResetBubbleManager();
}

// Test that a confirmation bubble is dropped if requested while another bubble
// is active.
TEST_F(BubbleManagerImplTest,
       RequestShow_ConfirmationBubbleWhileActive_IsDropped) {
  // Show a regular, non-confirmation bubble.
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager().RequestShowController(*card_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  // Create a confirmation bubble.
  std::unique_ptr<MockBubbleController> confirmation_controller =
      CreateController(BubbleType::kVirtualCardEnrollConfirmation,
                       /*can_be_reshown=*/false);

  // Attempt to show the confirmation bubble. It should not be shown or queued.
  EXPECT_CALL(*confirmation_controller, ShowBubble()).Times(0);
  bubble_manager().RequestShowController(*confirmation_controller,
                                         /*force_show=*/false);

  // The original bubble should still be active, and the confirmation bubble
  // should not be showing or pending.
  EXPECT_TRUE(card_controller->IsShowingBubble());
  EXPECT_FALSE(confirmation_controller->IsShowingBubble());
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      BubbleType::kVirtualCardEnrollConfirmation));
}

// Test that when a confirmation bubble is preempted, it is not re-queued.
TEST_F(BubbleManagerImplTest,
       RequestShow_PreemptConfirmationBubble_IsNotReQueued) {
  // Show a confirmation bubble.
  std::unique_ptr<MockBubbleController> confirmation_controller =
      CreateController(BubbleType::kVirtualCardEnrollConfirmation,
                       /*can_be_reshown=*/false);
  EXPECT_CALL(*confirmation_controller, ShowBubble());
  bubble_manager().RequestShowController(*confirmation_controller,
                                         /*force_show=*/false);
  ASSERT_TRUE(confirmation_controller->IsShowingBubble());

  // Create a higher-priority bubble to preempt the active one.
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);

  // Expect the confirmation bubble to be hidden and the new one shown.
  {
    InSequence sequence;
    EXPECT_CALL(*confirmation_controller,
                HideBubble(/*initiated_by_bubble_manager=*/true));
    EXPECT_CALL(*password_controller, ShowBubble());
  }

  // Show the higher-priority bubble.
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);

  // Verify the new bubble is active and the confirmation bubble is not.
  EXPECT_TRUE(password_controller->IsShowingBubble());
  EXPECT_FALSE(confirmation_controller->IsShowingBubble());

  // Verify the confirmation bubble was NOT added back to the queue.
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      BubbleType::kVirtualCardEnrollConfirmation));
}

// Test that a confirmation bubble is not queued if requested while the tab is
// inactive.
TEST_F(BubbleManagerImplTest,
       RequestShow_ConfirmationBubbleOnInactiveTab_IsNotQueued) {
  // Deactivate the tab.
  tab_interface()->Deactivate();

  // Create a confirmation bubble.
  std::unique_ptr<MockBubbleController> confirmation_controller =
      CreateController(BubbleType::kVirtualCardEnrollConfirmation,
                       /*can_be_reshown=*/false);

  // Try to show the bubble. Since the tab is inactive and it's a confirmation
  // bubble, it should be dropped entirely, not queued.
  EXPECT_CALL(*confirmation_controller, ShowBubble()).Times(0);
  bubble_manager().RequestShowController(*confirmation_controller,
                                         /*force_show=*/false);

  // Verify it wasn't queued.
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      BubbleType::kVirtualCardEnrollConfirmation));

  // Reactivating the tab should not trigger the bubble to show.
  tab_interface()->Activate();
  EXPECT_FALSE(confirmation_controller->IsShowingBubble());
}

// Tests that if the web contents is deactivated, the show bubble request leads
// to bubble getting added to the queue.
TEST_F(BubbleManagerImplTest, TabDeactivated_ShowAddsToQueue) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Simulate the tab becoming hidden.
  tab_interface()->Deactivate();

  bubble_manager().RequestShowController(*address_controller,
                                         /*force_show=*/false);
  EXPECT_FALSE(address_controller->IsShowingBubble());

  EXPECT_CALL(*address_controller, ShowBubble());

  // Simulate the tab becoming visible.
  tab_interface()->Activate();

  EXPECT_TRUE(address_controller->IsShowingBubble());

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bubble.Queue.ShownFromQueueOnTabVisible",
      BubbleType::kSaveUpdateAddress, 1);
}

// Test that HasPendingBubbleOfSameType returns true for a preempting bubble
// type (like Passwords), while HasConflictingPendingBubble returns false.
TEST_F(BubbleManagerImplTest, PreemptingType_IsPending_ButDoesNotBlock) {
  std::unique_ptr<MockBubbleController> filled_card_controller =
      CreateController(BubbleType::kFilledCardInformation);
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);

  // Show a high-priority bubble to ensure the next one is queued.
  EXPECT_CALL(*filled_card_controller, ShowBubble());
  bubble_manager().RequestShowController(*filled_card_controller,
                                         /*force_show=*/false);

  // Queue the password bubble.
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);

  EXPECT_TRUE(bubble_manager().HasPendingBubbleOfSameType(
      password_controller->GetBubbleType()));
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      password_controller->GetBubbleType()));
}

// Test that both HasPendingBubbleOfSameType and HasConflictingPendingBubble
// return false if the bubble has timed out.
TEST_F(BubbleManagerImplTest,
       PreemptingType_TimedOut_NeitherPendingNorBlocking) {
  std::unique_ptr<MockBubbleController> filled_card_controller =
      CreateController(BubbleType::kFilledCardInformation);
  std::unique_ptr<MockBubbleController> password_controller =
      CreateController(BubbleType::kPassword);

  // Show a high-priority bubble.
  EXPECT_CALL(*filled_card_controller, ShowBubble());
  bubble_manager().RequestShowController(*filled_card_controller,
                                         /*force_show=*/false);

  // Queue the password bubble.
  bubble_manager().RequestShowController(*password_controller,
                                         /*force_show=*/false);

  // Fast forward time past the timeout.
  FastForwardBy(base::Seconds(3601));

  // Both should now return false.
  EXPECT_FALSE(bubble_manager().HasPendingBubbleOfSameType(
      password_controller->GetBubbleType()));
  EXPECT_FALSE(bubble_manager().HasConflictingPendingBubble(
      password_controller->GetBubbleType()));
}

}  // namespace autofill
