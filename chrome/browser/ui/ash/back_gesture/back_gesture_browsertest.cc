// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/containers/contains.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"

namespace {

// The class to record received gesture events and key events to verify if back
// gesture has been performed.
class BackGestureEventRecorder : public ui::EventHandler {
 public:
  BackGestureEventRecorder() = default;
  BackGestureEventRecorder(const BackGestureEventRecorder&) = delete;
  BackGestureEventRecorder& operator=(const BackGestureEventRecorder&) = delete;
  ~BackGestureEventRecorder() override = default;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    received_event_types_.insert(event->type());
    if (wait_for_event_ == event->type() && run_loop_) {
      run_loop_->Quit();
      wait_for_event_ = ui::EventType::kUnknown;
    }
  }

  void OnKeyEvent(ui::KeyEvent* event) override {
    // If back gesture can be performed, a ui::VKEY_BROWSR_BACK key pressed and
    // key released will be generated.
    received_event_types_.insert(event->type());
  }

  void WaitUntilReceivedGestureEvent(ui::EventType event_type) {
    wait_for_event_ = event_type;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  bool HasReceivedEvent(ui::EventType event_type) {
    return base::Contains(received_event_types_, event_type);
  }

  void Reset() {
    received_event_types_.clear();
    wait_for_event_ = ui::EventType::kUnknown;
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  // Stores the event types of the received gesture events.
  base::flat_set<ui::EventType> received_event_types_;
  ui::EventType wait_for_event_ = ui::EventType::kUnknown;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class BackGestureBrowserTest : public InProcessBrowserTest {
 public:
  BackGestureBrowserTest() = default;
  BackGestureBrowserTest(const BackGestureBrowserTest&) = delete;
  BackGestureBrowserTest& operator=(const BackGestureBrowserTest&) = delete;
  ~BackGestureBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/ash/back_gesture"));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Enter tablet mode.
    ash::TabletModeControllerTestApi().EnterTabletMode();
    ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  }

  content::RenderWidgetHost* GetRenderWidgetHost() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetRenderWidgetHostView()
        ->GetRenderWidgetHost();
  }

  // Navigate to |url| and wait until browser thread is synchronized with render
  // thread. It's needed so that the touch action is correctly initialized.
  void NavigateToURLAndWaitForMainFrame(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
    frame_observer.Wait();
  }
};

// Test back gesture behavior with different touch actions.
IN_PROC_BROWSER_TEST_F(BackGestureBrowserTest, TouchActions) {
  // Navigate to a page with {touch-action: none} defined.
  NavigateToURLAndWaitForMainFrame(
      embedded_test_server()->GetURL("/page_touch_action_none.html"));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  const gfx::Rect bounds = browser()->window()->GetBounds();
  const gfx::Point start_point = bounds.left_center();
  const gfx::Point end_point =
      gfx::Point(start_point.x() + 200, start_point.y());

  BackGestureEventRecorder recorder;
  browser_window->AddPreTargetHandler(&recorder);

  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  event_generator.set_current_screen_location(start_point);
  event_generator.PressTouch();
  event_generator.MoveTouch(end_point);
  event_generator.ReleaseTouch();
  recorder.WaitUntilReceivedGestureEvent(ui::EventType::kGestureEnd);

  // BackGestureEventHandler did not handle gesture scroll events, so |recorder|
  // should be able to get the scroll events.
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureTapDown));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollBegin));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollUpdate));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollEnd) ||
              recorder.HasReceivedEvent(ui::EventType::kScrollFlingStart));
  // ui::VKEY_BROWSR_BACK key pressed and key released will not be generated
  // because back operation is not performed.
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kKeyPressed));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kKeyReleased));

  recorder.Reset();

  // Now navigate to a page with {touch-action: auto} defined.
  NavigateToURLAndWaitForMainFrame(
      embedded_test_server()->GetURL("/page_touch_action_auto.html"));

  event_generator.set_current_screen_location(start_point);
  event_generator.PressTouch();
  event_generator.MoveTouch(end_point);
  event_generator.ReleaseTouch();
  recorder.WaitUntilReceivedGestureEvent(ui::EventType::kGestureEnd);

  // BackGestureEventHandler has handled gesture scroll events, so |recorder|
  // should not be able to get the scroll events.
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kGestureTapDown));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollBegin));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollUpdate));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollEnd));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kScrollFlingStart));
  // ui::VKEY_BROWSR_BACK key pressed and key released will be generated because
  // back operation can be performed.
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kKeyPressed));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kKeyReleased));
  browser_window->RemovePreTargetHandler(&recorder);
}

// Test the back gesture behavior on page that has prevented default touch
// behavior.
IN_PROC_BROWSER_TEST_F(BackGestureBrowserTest, PreventDefault) {
  NavigateToURLAndWaitForMainFrame(
      embedded_test_server()->GetURL("/page_prevent_default.html"));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  const gfx::Rect bounds = browser()->window()->GetBounds();
  BackGestureEventRecorder recorder;
  browser_window->AddPreTargetHandler(&recorder);

  // Start drag on the page that prevents default touch behavior.
  const gfx::Point start_point = bounds.left_center();
  const gfx::Point end_point =
      gfx::Point(start_point.x() + 200, start_point.y());
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  event_generator.set_current_screen_location(start_point);
  event_generator.PressTouch();
  event_generator.MoveTouch(end_point);
  event_generator.ReleaseTouch();
  recorder.WaitUntilReceivedGestureEvent(ui::EventType::kGestureEnd);

  // ui::VKEY_BROWSR_BACK key pressed and key released will not be generated
  // because back operation is not performed.
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kKeyPressed));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kKeyReleased));
  browser_window->RemovePreTargetHandler(&recorder);
}
