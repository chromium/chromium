// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_event_handler_aura.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"

class WebUIBubbleEventHandlerAuraTest : public aura::test::AuraTestBase {
 public:
  // aura::test::AuraTestBase:
  void SetUp() override {
    AuraTestBase::SetUp();
    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(root_window());
    window_ = std::unique_ptr<aura::Window>(
        CreateNormalWindow(100, root_window(), &test_delegate_));
    window_->SetBounds(gfx::Rect(0, 0, 300, 300));
    window_->AddPreTargetHandler(&event_handler_);
  }
  void TearDown() override {
    window_.reset();
    event_generator_.reset();
    AuraTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  WebUIBubbleEventHandlerAura event_handler_;
  aura::test::TestWindowDelegate test_delegate_;
  std::unique_ptr<aura::Window> window_;
};

TEST_F(WebUIBubbleEventHandlerAuraTest, DragsInCaptionArea_Mouse) {
  test_delegate_.set_window_component(HTCAPTION);
  const gfx::Rect initial_screen_bounds = window_->GetBoundsInScreen();

  event_generator_->MoveMouseToCenterOf(window_.get());
  event_generator_->PressLeftButton();
  event_generator_->MoveMouseBy(10, 10);

  EXPECT_EQ(initial_screen_bounds + gfx::Vector2d(10, 10),
            window_->GetBoundsInScreen());
}

TEST_F(WebUIBubbleEventHandlerAuraTest, DragsInCaptionArea_Touch) {
  test_delegate_.set_window_component(HTCAPTION);
  const gfx::Rect initial_screen_bounds = window_->GetBoundsInScreen();

  const gfx::Point scroll_start = window_->GetBoundsInScreen().CenterPoint();
  const gfx::Point scroll_end = scroll_start + gfx::Vector2d(10, 10);
  event_generator_->GestureScrollSequence(scroll_start, scroll_end,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);

  EXPECT_EQ(initial_screen_bounds + gfx::Vector2d(10, 10),
            window_->GetBoundsInScreen());
}

TEST_F(WebUIBubbleEventHandlerAuraTest, DoesNotDragInClientArea_Mouse) {
  test_delegate_.set_window_component(HTCLIENT);
  const gfx::Rect initial_screen_bounds = window_->GetBoundsInScreen();

  event_generator_->MoveMouseToCenterOf(window_.get());
  event_generator_->PressLeftButton();
  event_generator_->MoveMouseBy(10, 10);

  EXPECT_EQ(initial_screen_bounds, window_->GetBoundsInScreen());
}

TEST_F(WebUIBubbleEventHandlerAuraTest, DoesNotDragInClientArea_Touch) {
  test_delegate_.set_window_component(HTCLIENT);
  const gfx::Rect initial_screen_bounds = window_->GetBoundsInScreen();

  const gfx::Point scroll_start = window_->GetBoundsInScreen().CenterPoint();
  const gfx::Point scroll_end = scroll_start + gfx::Vector2d(10, 10);
  event_generator_->GestureScrollSequence(scroll_start, scroll_end,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);

  EXPECT_EQ(initial_screen_bounds, window_->GetBoundsInScreen());
}
