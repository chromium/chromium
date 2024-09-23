// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"

#include <memory>
#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/views_test_base.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

using extensions::AppWindow;

class ShapedAppWindowTargeterTest : public views::ViewsTestBase {
 public:
  ShapedAppWindowTargeterTest() : web_view_(nullptr) {}

  ShapedAppWindowTargeterTest(const ShapedAppWindowTargeterTest&) = delete;
  ShapedAppWindowTargeterTest& operator=(const ShapedAppWindowTargeterTest&) =
      delete;

  ~ShapedAppWindowTargeterTest() override {}

  views::Widget* widget() { return widget_.get(); }

  extensions::NativeAppWindow* app_window() { return &app_window_; }
  ChromeNativeAppWindowViewsAura* app_window_views() { return &app_window_; }

 protected:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.remove_standard_frame = true;
    params.bounds = gfx::Rect(30, 30, 100, 100);
    params.context = root_window();
    widget_->Init(std::move(params));

    app_window_.set_web_view_for_testing(&web_view_);
    app_window_.set_window_for_testing(widget_.get());

    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  void SetWindowResizable(bool resizable) {
    widget_->GetNativeWindow()->SetProperty(
        aura::client::kResizeBehaviorKey,
        aura::client::kResizeBehaviorCanMaximize |
            aura::client::kResizeBehaviorCanMinimize |
            (resizable ? aura::client::kResizeBehaviorCanResize : 0));
  }

 private:
  views::WebView web_view_;
  std::unique_ptr<views::Widget> widget_;
  ChromeNativeAppWindowViewsAura app_window_;
};

TEST_F(ShapedAppWindowTargeterTest, HitTestBasic) {
  aura::Window* window = widget()->GetNativeWindow();
  {
    // Without any custom shapes, the event should be targeted correctly to the
    // window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(40, 40),
                        gfx::Point(40, 40), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }

  auto rects = std::make_unique<AppWindow::ShapeRects>();
  rects->emplace_back();
  app_window()->UpdateShape(std::move(rects));
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
  rects = std::make_unique<AppWindow::ShapeRects>();
  rects->emplace_back(40, 0, 20, 100);
  rects->emplace_back(0, 40, 100, 20);
  app_window()->UpdateShape(std::move(rects));
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
    EXPECT_EQ(window, move2.target());
  }
}

TEST_F(ShapedAppWindowTargeterTest, HitTestOnlyForShapedWindow) {
  // Install a window-targeter on the root window that allows a window to
  // receive events outside of its bounds. Verify that this window-targeter is
  // active unless the window has a custom shape.
  gfx::Insets inset(-30);
  root_window()->SetEventTargeter(
      std::make_unique<wm::EasyResizeWindowTargeter>(inset, inset));

  aura::Window* window = widget()->GetNativeWindow();
  {
    // Without any custom shapes, an event within the window bounds should be
    // targeted correctly to the window.
    ui::MouseEvent move_inside(ui::EventType::kMouseMoved, gfx::Point(40, 40),
                               gfx::Point(40, 40), ui::EventTimeForNow(),
                               ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details =
        GetEventSink()->OnEventFromSource(&move_inside);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move_inside.target());
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
    EXPECT_EQ(window, move.target());
  }

  auto rects = std::make_unique<AppWindow::ShapeRects>();
  rects->emplace_back(40, 0, 20, 100);
  rects->emplace_back(0, 40, 100, 20);
  app_window()->UpdateShape(std::move(rects));
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
  app_window()->UpdateShape(std::unique_ptr<AppWindow::ShapeRects>());
  SetWindowResizable(true);
  {
    ui::MouseEvent move(move_outside);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
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

// Tests targeting of events on a window with an EasyResizeWindowTargeter
// installed on its container.
TEST_F(ShapedAppWindowTargeterTest, ResizeInsetsWithinBounds) {
  aura::Window* window = widget()->GetNativeWindow();
  {
    // An event in the center of the window should always have
    // |window| as its target.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(80, 80),
                        gfx::Point(80, 80), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }
  {
    // Without an EasyResizeTargeter on the container, an event
    // inside the window and within 5px of an edge should have
    // |window| as its target.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(32, 37),
                        gfx::Point(32, 37), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // The non standard app frame has a easy resize targetter installed.
  std::unique_ptr<views::NonClientFrameView> frame(
      app_window_views()->CreateNonStandardAppFrame());
  {
    // Ensure that the window has an event targeter (there should be an
    // EasyResizeWindowTargeter installed).
    EXPECT_TRUE(static_cast<ui::EventTarget*>(window)->GetEventTargeter());
  }
  {
    // An event in the center of the window should always have
    // |window| as its target.
    // TODO(mgiuca): This isn't really testing anything (note that it has the
    // same expectation as the border case below). In the real environment, the
    // target will actually be the RenderWidgetHostViewAura's window that is the
    // child of the child of |window|, whereas in the border case it *will* be
    // |window|. However, since this test environment does not have a
    // RenderWidgetHostViewAura, we cannot differentiate the two cases. Fix
    // the test environment so that the test can assert that non-border events
    // bubble down to a child of |window|.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(80, 80),
                        gfx::Point(80, 80), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }
  {
    // With an EasyResizeTargeter on the container, an event
    // inside the window and within 5px of an edge should have
    // |window| as its target.
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(32, 37),
                        gfx::Point(32, 37), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }
#endif  // defined (OS_CHROMEOS)
}
