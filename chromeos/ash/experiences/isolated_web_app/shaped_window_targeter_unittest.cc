// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/shaped_window_targeter.h"

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

namespace ash {

class ShapedWindowTargeterTest : public aura::test::AuraTestBase {
 public:
  ShapedWindowTargeterTest() = default;

  aura::Window* window() { return window_.get(); }

 protected:
  void SetUp() override {
    aura::test::AuraTestBase::SetUp();
    window_ = std::make_unique<aura::Window>(&test_window_delegate_);
    window_->Init(ui::LAYER_NOT_DRAWN);
    window_->SetBounds(gfx::Rect(30, 30, 100, 100));
    root_window()->AddChild(window_.get());
    window_->Show();
  }

  void TearDown() override {
    window_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  void SetWindowResizable(bool resizable) {
    window_->SetProperty(
        aura::client::kResizeBehaviorKey,
        aura::client::kResizeBehaviorCanMaximize |
            aura::client::kResizeBehaviorCanMinimize |
            (resizable ? aura::client::kResizeBehaviorCanResize : 0));
  }

 private:
  aura::test::TestWindowDelegate test_window_delegate_;
  std::unique_ptr<aura::Window> window_;
};

TEST_F(ShapedWindowTargeterTest, HitTestBasic) {
  {
    // Without any custom shapes, the event should be targeted correctly to the
    // window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(40, 40),
                        gfx::Point(40, 40), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window(), move.target());
  }

  // Set an empty shape.
  window()->SetEventTargeter(
      std::make_unique<ShapedWindowTargeter>(std::vector<gfx::Rect>()));
  {
    // With an empty custom shape, all events within the window should fall
    // through to the root window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(40, 40),
                        gfx::Point(40, 40), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }

  // Window shape (global coordinates)
  //    30       70    90        130
  //  30 +        +-----+
  //       .      |     |             <- mouse move (40,40)
  //  70 +--------+     +---------+
  //     |           .            |   <- mouse move (80,80)
  //  90 +--------+     +---------+
  //              |     |
  // 130          +-----+
  std::vector<gfx::Rect> rects;
  rects.emplace_back(40, 0, 20, 100);
  rects.emplace_back(0, 40, 100, 20);
  window()->SetEventTargeter(std::make_unique<ShapedWindowTargeter>(rects));

  {
    // With the custom shape, the events that don't fall within the custom shape
    // will go through to the root window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(40, 40),
                        gfx::Point(40, 40), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());

    // But events within the shape will still reach the window.
    ui::MouseEvent move2(ui::EventType::kMouseMoved, gfx::Point(80, 80),
                         gfx::Point(80, 80), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    details = GetEventSink()->OnEventFromSource(&move2);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window(), move2.target());
  }
}

TEST_F(ShapedWindowTargeterTest, HitTestOnlyForShapedWindow) {
  // Install a window-targeter on the root window that allows a window to
  // receive events outside of its bounds. Verify that this window-targeter is
  // active unless the window has a custom shape.
  gfx::Insets inset(-30);
  root_window()->SetEventTargeter(
      std::make_unique<wm::EasyResizeWindowTargeter>(inset, inset));

  {
    // Without any custom shapes, an event within the window bounds should be
    // targeted correctly to the window.
    ui::MouseEvent move_inside(ui::EventType::kMouseMoved, gfx::Point(40, 40),
                               gfx::Point(40, 40), ui::EventTimeForNow(),
                               ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details =
        GetEventSink()->OnEventFromSource(&move_inside);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window(), move_inside.target());
  }

  ui::MouseEvent move_outside(ui::EventType::kMouseMoved, gfx::Point(10, 10),
                              gfx::Point(10, 10), ui::EventTimeForNow(),
                              ui::EF_NONE, ui::EF_NONE);
  SetWindowResizable(false);
  {
    // Without any custom shapes, an event that falls just outside the window
    // bounds should also be targeted correctly to the root window (for
    // non-resizable windows).
    ui::MouseEvent move(move_outside);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }

  SetWindowResizable(true);
  {
    // Without any custom shapes, an event that falls just outside the window
    // bounds should also be targeted correctly to the window, because of the
    // targeter installed on the root-window (for resizable windows).
    ui::MouseEvent move(move_outside);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window(), move.target());
  }

  std::vector<gfx::Rect> rects;
  rects.emplace_back(40, 0, 20, 100);
  rects.emplace_back(0, 40, 100, 20);
  window()->SetEventTargeter(std::make_unique<ShapedWindowTargeter>(rects));

  {
    // With the custom shape, the events that don't fall within the custom shape
    // will go through to the root window.
    ui::MouseEvent move(move_outside);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }

  // Remove the custom shape. This should restore the behaviour of targeting the
  // app window for events just outside its bounds (for a resizable window).
  window()->SetEventTargeter(nullptr);
  SetWindowResizable(true);
  {
    ui::MouseEvent move(move_outside);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window(), move.target());
  }

  // Make the window non-resizable. Events near (but outside) of the window
  // should reach the root window.
  SetWindowResizable(false);
  {
    ui::MouseEvent move(move_outside);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }
}

}  // namespace ash
