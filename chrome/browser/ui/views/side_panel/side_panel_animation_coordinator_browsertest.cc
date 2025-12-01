// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_animation_coordinator.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/views_test_utils.h"

using ::testing::_;

class MockSidePanelAnimationObserver
    : public SidePanelAnimationCoordinator::AnimationIdObserver,
      public SidePanelAnimationCoordinator::AnimationTypeObserver {
 public:
  MOCK_METHOD(
      void,
      OnAnimationSequenceProgressed,
      (const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id,
       double animation_value),
      (override));
  MOCK_METHOD(
      void,
      OnAnimationSequenceEnded,
      (const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id),
      (override));
  MOCK_METHOD(void,
              OnAnimationTypeEnded,
              (const SidePanelAnimationCoordinator::AnimationType),
              (override));
  MOCK_METHOD(void,
              OnAnimationTypeStarted,
              (const SidePanelAnimationCoordinator::AnimationType),
              (override));
};

class SidePanelAnimationCoordinatorBrowserTest : public InProcessBrowserTest {
 public:
  SidePanelAnimationCoordinator* GetAnimationCoordinator() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_height_side_panel()
        ->animation_coordinator();
  }

  void AddAnimationSequence(
      SidePanelAnimationCoordinator::AnimationType type,
      SidePanelAnimationCoordinator::AnimationSequence sequence) {
    auto& animation_spec_map =
        GetAnimationCoordinator()->animation_spec_map_for_testing();
    animation_spec_map.at(type).sequences.push_back(sequence);
  }
};

IN_PROC_BROWSER_TEST_F(SidePanelAnimationCoordinatorBrowserTest,
                       AnimationProgressedWithMismatchedAnimationIds) {
  SidePanelAnimationCoordinator* animation_coordinator =
      GetAnimationCoordinator();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnimationId);

  // Add a test animation that only exists for the kOpen animation type.
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kOpen,
                       {.animation_id = kTestAnimationId,
                        .start = base::Milliseconds(0),
                        .duration = base::Milliseconds(100)});

  MockSidePanelAnimationObserver control_observer;
  MockSidePanelAnimationObserver test_observer;
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpen, &control_observer);
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kClose, &control_observer);
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpen, &test_observer);

  // The bounds animation and the test animation should run when opening the
  // side panel.
  base::test::TestFuture<void> control_animation_ended;
  EXPECT_CALL(
      control_observer,
      OnAnimationTypeEnded(SidePanelAnimationCoordinator::AnimationType::kOpen))
      .WillOnce(
          [&control_animation_ended]() { control_animation_ended.SetValue(); });

  base::test::TestFuture<void> test_animation_ended;
  EXPECT_CALL(
      test_observer,
      OnAnimationTypeEnded(SidePanelAnimationCoordinator::AnimationType::kOpen))
      .WillOnce([&test_animation_ended]() { test_animation_ended.SetValue(); });

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  EXPECT_TRUE(control_animation_ended.Wait());
  EXPECT_TRUE(test_animation_ended.Wait());

  control_animation_ended.Clear();
  test_animation_ended.Clear();

  // Only the bounds animation should run when closing the side panel.
  EXPECT_CALL(control_observer,
              OnAnimationTypeEnded(
                  SidePanelAnimationCoordinator::AnimationType::kClose))
      .WillOnce(
          [&control_animation_ended]() { control_animation_ended.SetValue(); });

  EXPECT_CALL(test_observer,
              OnAnimationTypeEnded(
                  SidePanelAnimationCoordinator::AnimationType::kClose))
      .Times(0);

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kClose);

  EXPECT_TRUE(control_animation_ended.Wait());
}

IN_PROC_BROWSER_TEST_F(SidePanelAnimationCoordinatorBrowserTest,
                       AnimationIdObserversDoNotFireTypeCallbacks) {
  SidePanelAnimationCoordinator* animation_coordinator =
      GetAnimationCoordinator();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnimationId);
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kOpen,
                       {.animation_id = kTestAnimationId,
                        .start = base::Milliseconds(0),
                        .duration = base::Milliseconds(100)});
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kClose,
                       {.animation_id = kTestAnimationId,
                        .start = base::Milliseconds(0),
                        .duration = base::Milliseconds(100)});

  MockSidePanelAnimationObserver target_observer;
  animation_coordinator->AddObserver(kTestAnimationId, &target_observer);

  MockSidePanelAnimationObserver control_observer;
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpen, &control_observer);
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kClose, &control_observer);

  base::test::TestFuture<void> target_animation_finished;
  base::test::TestFuture<void> control_animation_finished;

  EXPECT_CALL(target_observer, OnAnimationTypeStarted(_)).Times(0);
  EXPECT_CALL(target_observer, OnAnimationTypeEnded(_)).Times(0);

  EXPECT_CALL(
      control_observer,
      OnAnimationTypeEnded(SidePanelAnimationCoordinator::AnimationType::kOpen))
      .WillOnce([&control_animation_finished]() {
        control_animation_finished.SetValue();
      });

  // Manually drive the animation to avoid flakiness.
  auto* animation = static_cast<gfx::SlideAnimation*>(
      animation_coordinator->animation_for_testing());
  auto* delegate = static_cast<gfx::AnimationDelegate*>(animation_coordinator);

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  // Progress the animation and verify the observer is fired at least once.
  EXPECT_CALL(target_observer,
              OnAnimationSequenceProgressed(kTestAnimationId, _))
      .Times(testing::AtLeast(1));

  // Manually drive the animation to 50% progress.
  animation->SetCurrentValue(0.5);
  delegate->AnimationProgressed(animation);

  // Go to the end of the test animation and verify it is observed properly.
  EXPECT_CALL(target_observer, OnAnimationSequenceEnded(kTestAnimationId))
      .WillOnce([&target_animation_finished]() {
        target_animation_finished.SetValue();
      });

  animation->End();

  EXPECT_TRUE(target_animation_finished.Wait());
  EXPECT_TRUE(control_animation_finished.Wait());

  target_animation_finished.Clear();
  control_animation_finished.Clear();

  EXPECT_CALL(target_observer,
              OnAnimationSequenceProgressed(kTestAnimationId, _))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(target_observer, OnAnimationSequenceEnded(kTestAnimationId))
      .WillOnce([&target_animation_finished]() {
        target_animation_finished.SetValue();
      });

  EXPECT_CALL(control_observer,
              OnAnimationTypeEnded(
                  SidePanelAnimationCoordinator::AnimationType::kClose))
      .WillOnce([&control_animation_finished]() {
        control_animation_finished.SetValue();
      });

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kClose);

  // Manually drive the animation to 50% progress.
  animation->SetCurrentValue(0.5);
  delegate->AnimationProgressed(animation);

  animation->End();

  EXPECT_TRUE(target_animation_finished.Wait());
  EXPECT_TRUE(control_animation_finished.Wait());
}

IN_PROC_BROWSER_TEST_F(SidePanelAnimationCoordinatorBrowserTest,
                       AnimationTypeObserversFireStartedAndEnded) {
  SidePanelAnimationCoordinator* animation_coordinator =
      GetAnimationCoordinator();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnimationId);

  // Add a test animation for open and close animation type.
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kOpen,
                       {.animation_id = kTestAnimationId,
                        .start = base::Milliseconds(0),
                        .duration = base::Milliseconds(100)});
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kClose,
                       {.animation_id = kTestAnimationId,
                        .start = base::Milliseconds(0),
                        .duration = base::Milliseconds(100)});

  MockSidePanelAnimationObserver test_observer;
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpen, &test_observer);
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kClose, &test_observer);

  base::test::TestFuture<void> animation_started;
  EXPECT_CALL(test_observer,
              OnAnimationTypeStarted(
                  SidePanelAnimationCoordinator::AnimationType::kOpen))
      .WillOnce([&animation_started]() { animation_started.SetValue(); });

  base::test::TestFuture<void> animation_ended;
  EXPECT_CALL(
      test_observer,
      OnAnimationTypeEnded(SidePanelAnimationCoordinator::AnimationType::kOpen))
      .WillOnce([&animation_ended]() { animation_ended.SetValue(); });

  EXPECT_CALL(test_observer, OnAnimationSequenceProgressed(_, _)).Times(0);
  EXPECT_CALL(test_observer, OnAnimationSequenceEnded(_)).Times(0);

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  EXPECT_TRUE(animation_started.Wait());
  EXPECT_TRUE(animation_ended.Wait());

  animation_started.Clear();
  animation_ended.Clear();

  EXPECT_CALL(test_observer,
              OnAnimationTypeStarted(
                  SidePanelAnimationCoordinator::AnimationType::kClose))
      .WillOnce([&animation_started]() { animation_started.SetValue(); });

  EXPECT_CALL(test_observer,
              OnAnimationTypeEnded(
                  SidePanelAnimationCoordinator::AnimationType::kClose))
      .WillOnce([&animation_ended]() { animation_ended.SetValue(); });

  EXPECT_CALL(test_observer, OnAnimationSequenceProgressed(_, _)).Times(0);
  EXPECT_CALL(test_observer, OnAnimationSequenceEnded(_)).Times(0);

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kClose);

  EXPECT_TRUE(animation_started.Wait());
  EXPECT_TRUE(animation_ended.Wait());
}

IN_PROC_BROWSER_TEST_F(SidePanelAnimationCoordinatorBrowserTest,
                       AnimationTypeObserversFireEndedWhenCanceled) {
  SidePanelAnimationCoordinator* animation_coordinator =
      GetAnimationCoordinator();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnimationId);

  // Add a test animation for open animation type.
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kOpen,
                       {.animation_id = kTestAnimationId,
                        .start = base::Milliseconds(0),
                        .duration = base::Milliseconds(100)});

  MockSidePanelAnimationObserver test_observer;
  animation_coordinator->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpen, &test_observer);

  base::test::TestFuture<void> animation_started;
  EXPECT_CALL(test_observer,
              OnAnimationTypeStarted(
                  SidePanelAnimationCoordinator::AnimationType::kOpen))
      .WillOnce([&animation_started]() { animation_started.SetValue(); });

  base::test::TestFuture<void> animation_canceled;
  EXPECT_CALL(
      test_observer,
      OnAnimationTypeEnded(SidePanelAnimationCoordinator::AnimationType::kOpen))
      .WillOnce([&animation_canceled]() { animation_canceled.SetValue(); });

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  EXPECT_TRUE(animation_started.Wait());

  animation_coordinator->Reset(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  EXPECT_TRUE(animation_canceled.Wait());
}

IN_PROC_BROWSER_TEST_F(SidePanelAnimationCoordinatorBrowserTest,
                       AnimationsWithDifferentStartTimesFireInOrder) {
  SidePanelAnimationCoordinator* animation_coordinator =
      GetAnimationCoordinator();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstAnimationId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondAnimationId);

  // Add two test animations for the open animation type with different start
  // times.
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kOpen,
                       {.animation_id = kFirstAnimationId,
                        .start = base::Milliseconds(0),
                        .duration = base::Milliseconds(50)});
  AddAnimationSequence(SidePanelAnimationCoordinator::AnimationType::kOpen,
                       {.animation_id = kSecondAnimationId,
                        .start = base::Milliseconds(100),
                        .duration = base::Milliseconds(50)});

  MockSidePanelAnimationObserver first_observer;
  MockSidePanelAnimationObserver second_observer;
  animation_coordinator->AddObserver(kFirstAnimationId, &first_observer);
  animation_coordinator->AddObserver(kSecondAnimationId, &second_observer);

  // Set a custom container to control the animation time.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  animation_coordinator->animation_for_testing()->SetContainer(container.get());
  gfx::AnimationContainerTestApi test_api(container.get());

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  // Step 1: Advance by 25ms.
  // Should trigger first_observer (0-50ms), but not second (100-150ms).
  base::test::TestFuture<void> first_animation_progressed;
  EXPECT_CALL(first_observer,
              OnAnimationSequenceProgressed(kFirstAnimationId, _))
      .Times(testing::AtLeast(1))
      .WillOnce([&first_animation_progressed]() {
        first_animation_progressed.SetValue();
      });
  EXPECT_CALL(second_observer,
              OnAnimationSequenceProgressed(kSecondAnimationId, _))
      .Times(0);

  test_api.IncrementTime(base::Milliseconds(25));
  EXPECT_TRUE(first_animation_progressed.Wait());

  testing::Mock::VerifyAndClearExpectations(&first_observer);
  testing::Mock::VerifyAndClearExpectations(&second_observer);

  // Step 2: Advance by another 100ms (Total 125ms).
  // First observer should be done (stopped at 50ms).
  // Second observer should be active (100-150ms).
  base::test::TestFuture<void> second_animation_progressed;
  EXPECT_CALL(second_observer,
              OnAnimationSequenceProgressed(kSecondAnimationId, _))
      .Times(testing::AtLeast(1))
      .WillOnce([&second_animation_progressed]() {
        second_animation_progressed.SetValue();
      });
  EXPECT_CALL(first_observer,
              OnAnimationSequenceProgressed(kFirstAnimationId, _))
      .Times(0);

  test_api.IncrementTime(base::Milliseconds(100));
  EXPECT_TRUE(second_animation_progressed.Wait());
}

IN_PROC_BROWSER_TEST_F(SidePanelAnimationCoordinatorBrowserTest,
                       CoordinatorUsesLinearTweenType) {
  SidePanelAnimationCoordinator* animation_coordinator =
      GetAnimationCoordinator();

  // Set a custom container to control the animation time.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  animation_coordinator->animation_for_testing()->SetContainer(container.get());
  gfx::AnimationContainerTestApi test_api(container.get());

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  base::TimeDelta duration =
      animation_coordinator->animation_spec_map_for_testing()
          .at(SidePanelAnimationCoordinator::AnimationType::kOpen)
          .GetAnimationDuration();

  // Verify at 1%
  test_api.IncrementTime(duration * 0.01);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.01);

  // Verify at 7% (Accumulated)
  test_api.IncrementTime(duration * 0.06);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.07);

  // Verify at 17% (Accumulated)
  test_api.IncrementTime(duration * 0.10);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.17);

  // Verify at 25% (Accumulated)
  test_api.IncrementTime(duration * 0.08);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.25);

  // Verify at 31% (Accumulated)
  test_api.IncrementTime(duration * 0.06);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.31);

  // Verify at 50% (Accumulated)
  test_api.IncrementTime(duration * 0.19);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.5);

  // Verify at 63% (Accumulated)
  test_api.IncrementTime(duration * 0.13);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.63);

  // Verify at 75% (Accumulated)
  test_api.IncrementTime(duration * 0.12);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.75);

  // Verify at 95% (Accumulated)
  test_api.IncrementTime(duration * 0.20);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            0.95);

  // Verify at 100% (Accumulated)
  test_api.IncrementTime(duration * 0.05);
  EXPECT_EQ(animation_coordinator->animation_for_testing()->GetCurrentValue(),
            1.0);
}
