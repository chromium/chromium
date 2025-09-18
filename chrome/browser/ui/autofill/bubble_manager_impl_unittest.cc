// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/bubble_manager_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

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
      BubbleType bubble_type) {
    std::unique_ptr<MockBubbleController> controller =
        std::make_unique<MockBubbleController>();
    controller->SetBubbleType(bubble_type);
    return controller;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  BubbleManagerImpl bubble_manager_;
};

// Test that requesting a bubble when none is active shows it immediately.
TEST_F(BubbleManagerImplTest, RequestShow_NoActiveBubble_ShowsImmediately) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  EXPECT_CALL(*address_controller, ShowBubble());
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);
  EXPECT_TRUE(address_controller->IsShowingBubble());
}

// Test that a higher-priority bubble preempts a lower-priority one.
TEST_F(BubbleManagerImplTest, RequestShow_HigherPriority_PreemptsActive) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  EXPECT_CALL(*address_controller, ShowBubble());
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);
  ASSERT_TRUE(address_controller->IsShowingBubble());

  {
    InSequence sequence;
    EXPECT_CALL(*address_controller, HideBubble(/*show_next_bubble=*/false));
    EXPECT_CALL(*card_controller, ShowBubble());
  }

  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);
  EXPECT_FALSE(address_controller->IsShowingBubble());
  EXPECT_TRUE(card_controller->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);
  ASSERT_FALSE(address_controller->IsShowingBubble());

  // When the active (card) bubble is hidden, the address bubble should be shown
  // from the queue.
  EXPECT_CALL(*address_controller, ShowBubble());

  // Hide the card bubble.
  bubble_manager_.OnBubbleHiddenByController(*card_controller,
                                             /*show_next_bubble=*/true);

  // The state of the card controller should now be false.
  card_controller->HideBubble(/*show_next_bubble=*/false);

  EXPECT_FALSE(card_controller->IsShowingBubble());
  EXPECT_TRUE(address_controller->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);
  ASSERT_FALSE(address_controller->IsShowingBubble());

  // Request a high-priority password bubble. This will preempt the active
  // card bubble.
  {
    InSequence sequence;
    EXPECT_CALL(*card_controller, HideBubble(/*show_next_bubble=*/false));
    EXPECT_CALL(*password_controller, ShowBubble());
  }

  // Ensure that the queued address bubble is never shown during this process.
  EXPECT_CALL(*address_controller, ShowBubble()).Times(0);
  bubble_manager_.RequestShowController(*password_controller,
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
  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  EXPECT_CALL(*address_controller, ShowBubble()).Times(0);
  EXPECT_CALL(*card_controller, HideBubble(/*show_next_bubble=*/true)).Times(0);
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);

  EXPECT_TRUE(card_controller->IsShowingBubble());
  EXPECT_FALSE(address_controller->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*password_controller_1,
                                        /*force_show=*/false);
  ASSERT_TRUE(password_controller_1->IsShowingBubble());

  {
    InSequence sequence;
    EXPECT_CALL(*password_controller_1, HideBubble(/*show_next_bubble=*/false));
    EXPECT_CALL(*password_controller_2, ShowBubble());
  }

  bubble_manager_.RequestShowController(*password_controller_2,
                                        /*force_show=*/false);
  EXPECT_FALSE(password_controller_1->IsShowingBubble());
  EXPECT_TRUE(password_controller_2->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*password_controller,
                                        /*force_show=*/false);
  bubble_manager_.RequestShowController(*address_controller_1,
                                        /*force_show=*/false);
  bubble_manager_.RequestShowController(*address_controller_2,
                                        /*force_show=*/false);

  bubble_manager_.OnBubbleHiddenByController(*password_controller,
                                             /*show_next_bubble=*/true);

  EXPECT_TRUE(address_controller_1->IsShowingBubble());
  EXPECT_FALSE(address_controller_2->IsShowingBubble());

  // Ensure `address_controller_2` is never shown.
  bubble_manager_.OnBubbleHiddenByController(*address_controller_1,
                                             /*show_next_bubble=*/true);
  EXPECT_FALSE(address_controller_2->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*password_controller,
                                        /*force_show=*/false);
  bubble_manager_.RequestShowController(*address_controller_1,
                                        /*force_show=*/false);

  task_environment_.FastForwardBy(base::Seconds(3601));
  bubble_manager_.RequestShowController(*address_controller_2,
                                        /*force_show=*/false);

  bubble_manager_.OnBubbleHiddenByController(*password_controller,
                                             /*show_next_bubble=*/true);

  EXPECT_FALSE(address_controller_1->IsShowingBubble());
  EXPECT_TRUE(address_controller_2->IsShowingBubble());

  // Ensure `address_controller_1` is never shown.
  bubble_manager_.OnBubbleHiddenByController(*address_controller_2,
                                             /*show_next_bubble=*/true);
  EXPECT_FALSE(address_controller_1->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*filled_card_controller,
                                        /*force_show=*/false);
  bubble_manager_.RequestShowController(*password_controller_1,
                                        /*force_show=*/false);
  bubble_manager_.RequestShowController(*password_controller_2,
                                        /*force_show=*/false);

  bubble_manager_.OnBubbleHiddenByController(*filled_card_controller,
                                             /*show_next_bubble=*/true);
  EXPECT_FALSE(password_controller_1->IsShowingBubble());
  EXPECT_TRUE(password_controller_2->IsShowingBubble());

  // Ensure `password_controller_1` is never shown.
  bubble_manager_.OnBubbleHiddenByController(*password_controller_2,
                                             /*show_next_bubble=*/true);
  EXPECT_FALSE(password_controller_1->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);
  ASSERT_TRUE(address_controller->IsShowingBubble());

  // Simulate mouse hover.
  ON_CALL(*address_controller, IsMouseHovered).WillByDefault(Return(true));

  // Card bubble should not be shown, address bubble should not be hidden.
  EXPECT_CALL(*card_controller, ShowBubble()).Times(0);
  EXPECT_CALL(*address_controller, HideBubble(/*show_next_bubble=*/true))
      .Times(0);
  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);

  EXPECT_TRUE(address_controller->IsShowingBubble());
  EXPECT_FALSE(card_controller->IsShowingBubble());
}

// Test that HasPendingBubble returns false when no bubble is pending.
TEST_F(BubbleManagerImplTest, HasPendingBubble_NoBubble_ReturnsFalse) {
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);
  EXPECT_FALSE(bubble_manager_.HasPendingBubbleOfSameType(
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
  bubble_manager_.RequestShowController(*password_controller,
                                        /*force_show=*/false);

  // Queue the address bubble.
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);

  // Check that the address bubble is correctly reported as pending.
  EXPECT_TRUE(bubble_manager_.HasPendingBubbleOfSameType(
      address_controller->GetBubbleType()));
  // Check that a bubble type not in the queue returns false.
  EXPECT_FALSE(bubble_manager_.HasPendingBubbleOfSameType(
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
  bubble_manager_.RequestShowController(*password_controller,
                                        /*force_show=*/false);

  // Queue the address bubble and confirm it's pending.
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);
  ASSERT_TRUE(bubble_manager_.HasPendingBubbleOfSameType(
      address_controller->GetBubbleType()));

  // Fast forward time past the timeout.
  task_environment_.FastForwardBy(base::Seconds(3601));

  // Now, checking for the bubble should return false because it has timed out.
  EXPECT_FALSE(bubble_manager_.HasPendingBubbleOfSameType(
      address_controller->GetBubbleType()));
}

// Test that a force_show request preempts a higher-priority active bubble.
TEST_F(BubbleManagerImplTest, RequestShow_ForceShow_PreemptsActiveBubble) {
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  std::unique_ptr<MockBubbleController> address_controller =
      CreateController(BubbleType::kSaveUpdateAddress);

  // Show the high-priority bubble first.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  // Expect the active bubble to be hidden and the new one shown.
  {
    InSequence sequence;
    EXPECT_CALL(*card_controller, HideBubble(/*show_next_bubble=*/false));
    EXPECT_CALL(*address_controller, ShowBubble());
  }

  // Force show the lower-priority bubble. It should preempt the active one.
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/true);
  EXPECT_FALSE(card_controller->IsShowingBubble());
  EXPECT_TRUE(address_controller->IsShowingBubble());
}

// Test that the active bubble is hidden and queued when the tab visibility
// changes to hidden.
TEST_F(BubbleManagerImplTest, OnVisibilityChanged_Hidden_HidesAndQueuesBubble) {
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);

  // Show a bubble, making it active.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());

  // Expect the bubble to be hidden when visibility changes.
  EXPECT_CALL(*card_controller, HideBubble(/*show_next_bubble=*/false));

  // Simulate the tab becoming hidden.
  bubble_manager_.OnVisibilityChanged(content::Visibility::HIDDEN);

  // The bubble should no longer be considered "showing".
  EXPECT_FALSE(card_controller->IsShowingBubble());

  // To verify it was queued, we can simulate the tab becoming visible again.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager_.OnVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_TRUE(card_controller->IsShowingBubble());
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
  bubble_manager_.RequestShowController(*card_controller, /*force_show=*/false);
  bubble_manager_.RequestShowController(*address_controller,
                                        /*force_show=*/false);
  ASSERT_TRUE(card_controller->IsShowingBubble());
  ASSERT_FALSE(address_controller->IsShowingBubble());

  // Hide the active bubble and simulate the tab becoming hidden.
  // This leaves the address bubble in the queue.
  EXPECT_CALL(*card_controller, HideBubble(/*show_next_bubble=*/false));
  bubble_manager_.OnVisibilityChanged(content::Visibility::HIDDEN);
  ASSERT_FALSE(card_controller->IsShowingBubble());

  // When the tab becomes visible again, the card bubble should show again as
  // it has higher priority.
  EXPECT_CALL(*card_controller, ShowBubble());
  bubble_manager_.OnVisibilityChanged(content::Visibility::VISIBLE);

  EXPECT_TRUE(card_controller->IsShowingBubble());
}

// Test that no action is taken when the tab becomes hidden and there is no
// active bubble.
TEST_F(BubbleManagerImplTest, OnVisibilityChanged_Hidden_NoActiveBubble) {
  // Ensure no mock calls are expected on any controllers.
  // We can create a controller but never show it.
  std::unique_ptr<MockBubbleController> card_controller =
      CreateController(BubbleType::kSaveUpdateCard);
  EXPECT_CALL(*card_controller, HideBubble(_)).Times(0);

  // Simulate the tab becoming hidden. The test will fail if any unexpected
  // methods are called.
  bubble_manager_.OnVisibilityChanged(content::Visibility::HIDDEN);
}

}  // namespace autofill
