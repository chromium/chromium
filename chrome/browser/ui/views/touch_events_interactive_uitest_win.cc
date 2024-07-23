// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/aura/env.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/gestures/gesture_recognizer_impl.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace {

class TouchEventHandler : public ui::EventHandler {
 public:
  TouchEventHandler()
      : call_depth_(0),
        max_call_depth_(0),
        num_touch_presses_(0),
        num_pointers_down_(0),
        recursion_enabled_(false) {}

  TouchEventHandler(const TouchEventHandler&) = delete;
  TouchEventHandler& operator=(const TouchEventHandler&) = delete;

  ~TouchEventHandler() override {}

  // OnTouchEvent will simulate a second touch event (at |touch_point|) to force
  // recursion in event handling.
  void ForceRecursionInEventHandler(const gfx::Point& touch_point) {
    recursion_enabled_ = true;
    touch_point_ = touch_point;
  }

  void WaitForIdle() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    run_loop.RunUntilIdle();
  }
  void WaitForEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  int max_call_depth() const { return max_call_depth_; }

  int num_touch_presses() const { return num_touch_presses_; }

  int num_pointers_down() const { return num_pointers_down_; }

  void set_touch_point(const gfx::Point& pt) { touch_point_ = pt; }

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override {
    max_call_depth_ = std::max(++call_depth_, max_call_depth_);

    if (recursion_enabled_ &&
        (event->type() == ui::EventType::kTouchReleased)) {
      recursion_enabled_ = false;
      ui_controls::SendTouchEvents(ui_controls::kTouchPress, 1,
                                   touch_point_.x(), touch_point_.y());
      WaitForIdle();
    }

    switch (event->type()) {
      case ui::EventType::kTouchPressed:
        num_touch_presses_++;
        num_pointers_down_++;
        break;
      case ui::EventType::kTouchReleased:
        num_pointers_down_--;
        if (!quit_closure_.is_null() && num_pointers_down_ == 0) {
          quit_closure_.Run();
        }
        break;
      default:
        break;
    }
    --call_depth_;
  }

  int call_depth_;
  int max_call_depth_;
  int num_touch_presses_;
  int num_pointers_down_;
  base::RepeatingClosure quit_closure_;
  bool recursion_enabled_;
  gfx::Point touch_point_;
};

class TestingGestureRecognizer : public ui::GestureRecognizerImpl {
 public:
  TestingGestureRecognizer() = default;

  TestingGestureRecognizer(const TestingGestureRecognizer&) = delete;
  TestingGestureRecognizer& operator=(const TestingGestureRecognizer&) = delete;

  ~TestingGestureRecognizer() override = default;

  int num_touch_press_events() const { return num_touch_press_events_; }
  int num_touch_release_events() const { return num_touch_release_events_; }

 protected:
  // Overriden from GestureRecognizerImpl:
  bool ProcessTouchEventPreDispatch(ui::TouchEvent* event,
                                    ui::GestureConsumer* consumer) override {
    switch (event->type()) {
      case ui::EventType::kTouchPressed:
        num_touch_press_events_++;
        break;
      case ui::EventType::kTouchReleased:
        num_touch_release_events_++;
        break;
      default:
        break;
    }

    return ui::GestureRecognizerImpl::ProcessTouchEventPreDispatch(event,
                                                                   consumer);
  }

 private:
  int num_touch_press_events_ = 0;
  int num_touch_release_events_ = 0;
};

}  // namespace

class TouchEventsViewTest : public ViewEventTestBase {
 public:
  TouchEventsViewTest() = default;

  TouchEventsViewTest(const TouchEventsViewTest&) = delete;
  TouchEventsViewTest& operator=(const TouchEventsViewTest&) = delete;

  // ViewEventTestBase:
  void SetUp() override {
    ViewEventTestBase::SetUp();

    auto gesture_recognizer = std::make_unique<TestingGestureRecognizer>();
    gesture_recognizer_ = gesture_recognizer.get();
    aura::test::EnvTestHelper().SetGestureRecognizer(
        std::move(gesture_recognizer));
  }

  std::unique_ptr<views::View> CreateContentsView() override {
    auto touch_view = std::make_unique<views::View>();
    touch_view_ = touch_view.get();
    return touch_view;
  }

  gfx::Size GetPreferredSizeForContents() const override {
    return gfx::Size(600, 600);
  }

  void DoTestOnMessageLoop() override {
    const int touch_pointer_count = 3;
    TouchEventHandler touch_event_handler;
    window()->GetNativeWindow()->GetHost()->window()->AddPreTargetHandler(
        &touch_event_handler);
    gfx::Point in_content(touch_view_->width() / 2, touch_view_->height() / 2);
    views::View::ConvertPointToScreen(touch_view_, &in_content);

    ASSERT_TRUE(ui_controls::SendTouchEvents(ui_controls::kTouchPress,
                                             touch_pointer_count,
                                             in_content.x(), in_content.y()));
    touch_event_handler.WaitForIdle();
    touch_event_handler.WaitForEvents();
    EXPECT_EQ(touch_pointer_count, touch_event_handler.num_touch_presses());
    EXPECT_EQ(0, touch_event_handler.num_pointers_down());

    EXPECT_EQ(touch_pointer_count,
              gesture_recognizer_->num_touch_press_events());
    EXPECT_EQ(touch_pointer_count,
              gesture_recognizer_->num_touch_release_events());

    window()->GetNativeWindow()->GetHost()->window()->RemovePreTargetHandler(
        &touch_event_handler);
    Done();
  }

 protected:
  raw_ptr<views::View> touch_view_ = nullptr;
  raw_ptr<TestingGestureRecognizer> gesture_recognizer_ = nullptr;
  raw_ptr<ui::GestureRecognizer> initial_gr_ = nullptr;
};

#if BUILDFLAG(IS_WIN)  // Fails on latest versions of Windows.
                       // https://crbug.com/1108551.
#define MAYBE_CheckWindowsNativeMessageForTouchEvents \
  DISABLED_CheckWindowsNativeMessageForTouchEvents
#else
#define MAYBE_CheckWindowsNativeMessageForTouchEvents \
  CheckWindowsNativeMessageForTouchEvents
#endif
VIEW_TEST(TouchEventsViewTest, MAYBE_CheckWindowsNativeMessageForTouchEvents)

class TouchEventsRecursiveViewTest : public TouchEventsViewTest {
 public:
  TouchEventsRecursiveViewTest() {}

  TouchEventsRecursiveViewTest(const TouchEventsRecursiveViewTest&) = delete;
  TouchEventsRecursiveViewTest& operator=(const TouchEventsRecursiveViewTest&) =
      delete;

  void DoTestOnMessageLoop() override {
    const int touch_pointer_count = 1;
    TouchEventHandler touch_event_handler;
    window()->GetNativeWindow()->GetHost()->window()->AddPreTargetHandler(
        &touch_event_handler);
    gfx::Point in_content(touch_view_->width() / 2, touch_view_->height() / 2);
    views::View::ConvertPointToScreen(touch_view_, &in_content);
    touch_event_handler.ForceRecursionInEventHandler(in_content);

    ASSERT_TRUE(ui_controls::SendTouchEvents(ui_controls::kTouchPress,
                                             touch_pointer_count,
                                             in_content.x(), in_content.y()));
    touch_event_handler.WaitForEvents();

    EXPECT_EQ(touch_pointer_count + 1, touch_event_handler.num_touch_presses());
    EXPECT_EQ(0, touch_event_handler.num_pointers_down());
    EXPECT_EQ(2, touch_event_handler.max_call_depth());
    window()->GetNativeWindow()->GetHost()->window()->RemovePreTargetHandler(
        &touch_event_handler);
    Done();
  }
};

#if BUILDFLAG(IS_WIN)  // Fails on latest versions of Windows.
                       // https://crbug.com/1108551.
#define MAYBE_CheckWindowsRecursiveHandler DISABLED_CheckWindowsRecursiveHandler
#else
#define MAYBE_CheckWindowsRecursiveHandler CheckWindowsRecursiveHandler
#endif
VIEW_TEST(TouchEventsRecursiveViewTest, MAYBE_CheckWindowsRecursiveHandler)
