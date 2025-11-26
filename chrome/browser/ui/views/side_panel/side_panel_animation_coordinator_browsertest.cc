// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_animation_coordinator.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/views_test_utils.h"

using ::testing::_;

class MockSidePanelAnimationObserver
    : public SidePanelAnimationCoordinator::Observer {
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
};

class SidePanelAnimationCoordinatorTest : public InProcessBrowserTest {
 public:
  SidePanelAnimationCoordinator* GetAnimationCoordinator() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_height_side_panel()
        ->animation_coordinator();
  }
};

IN_PROC_BROWSER_TEST_F(SidePanelAnimationCoordinatorTest,
                       AnimationProgressedWithMismatchedAnimationIds) {
  SidePanelAnimationCoordinator* animation_coordinator =
      GetAnimationCoordinator();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnimationId);

  // Add a test animation that only exists for the kOpen animation type.
  auto& animation_spec_map =
      animation_coordinator->animation_spec_map_for_testing();
  auto& open_sequences =
      animation_spec_map.at(SidePanelAnimationCoordinator::AnimationType::kOpen)
          .sequences;
  open_sequences.push_back({.animation_id = kTestAnimationId,
                            .start = base::Milliseconds(0),
                            .duration = base::Milliseconds(100)});

  MockSidePanelAnimationObserver bounds_observer;
  MockSidePanelAnimationObserver test_observer;
  animation_coordinator->AddObserver(kSidePanelBoundsAnimation,
                                     &bounds_observer);
  animation_coordinator->AddObserver(kTestAnimationId, &test_observer);

  // The bounds animation and the test animation should run when opening the
  // side panel.
  base::test::TestFuture<void> bounds_animation_ended;
  EXPECT_CALL(bounds_observer,
              OnAnimationSequenceEnded(kSidePanelBoundsAnimation))
      .WillOnce(
          [&bounds_animation_ended]() { bounds_animation_ended.SetValue(); });

  base::test::TestFuture<void> test_animation_ended;
  EXPECT_CALL(test_observer, OnAnimationSequenceEnded(kTestAnimationId))
      .WillOnce([&test_animation_ended]() { test_animation_ended.SetValue(); });

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kOpen);

  EXPECT_TRUE(bounds_animation_ended.Wait());
  EXPECT_TRUE(test_animation_ended.Wait());

  bounds_animation_ended.Clear();
  test_animation_ended.Clear();

  // Only the bounds animation should run when closing the side panel.
  EXPECT_CALL(bounds_observer,
              OnAnimationSequenceEnded(kSidePanelBoundsAnimation))
      .WillOnce(
          [&bounds_animation_ended]() { bounds_animation_ended.SetValue(); });

  EXPECT_CALL(test_observer, OnAnimationSequenceEnded(kTestAnimationId))
      .Times(0);

  animation_coordinator->Start(
      SidePanelAnimationCoordinator::AnimationType::kClose);

  EXPECT_TRUE(bounds_animation_ended.Wait());
}
