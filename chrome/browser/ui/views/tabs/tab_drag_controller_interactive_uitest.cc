// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/aura/env.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_targeter.h"
#endif

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::WebContents;
using display::Display;
using ui_test_utils::GetDisplays;

namespace test {

namespace {

const char kTabDragControllerInteractiveUITestUserDataKey[] =
    "TabDragControllerInteractiveUITestUserData";

class TabDragControllerInteractiveUITestUserData
    : public base::SupportsUserData::Data {
 public:
  explicit TabDragControllerInteractiveUITestUserData(int id) : id_(id) {}
  ~TabDragControllerInteractiveUITestUserData() override {}
  int id() { return id_; }

 private:
  int id_;
};

#if defined(OS_CHROMEOS)
// Returns the window which stores window properties for this test. In mash, the
// properties are stored in the root window of the browser window.
aura::Window* GetWindowForProperties(aura::Window* window) {
  if (features::IsUsingWindowService())
    return window->GetRootWindow();
  return window;
}

aura::Window* GetWindowForTabStrip(TabStrip* tab_strip) {
  return tab_strip
             ? GetWindowForProperties(tab_strip->GetWidget()->GetNativeWindow())
             : nullptr;
}
#endif

}  // namespace

class QuitDraggingObserver : public content::NotificationObserver {
 public:
  QuitDraggingObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE, type);
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
    delete this;
  }

 private:
  ~QuitDraggingObserver() override {}

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(QuitDraggingObserver);
};

void SetID(WebContents* web_contents, int id) {
  web_contents->SetUserData(
      &kTabDragControllerInteractiveUITestUserDataKey,
      std::make_unique<TabDragControllerInteractiveUITestUserData>(id));
}

void ResetIDs(TabStripModel* model, int start) {
  for (int i = 0; i < model->count(); ++i)
    SetID(model->GetWebContentsAt(i), start + i);
}

std::string IDString(TabStripModel* model) {
  std::string result;
  for (int i = 0; i < model->count(); ++i) {
    if (i != 0)
      result += " ";
    WebContents* contents = model->GetWebContentsAt(i);
    TabDragControllerInteractiveUITestUserData* user_data =
        static_cast<TabDragControllerInteractiveUITestUserData*>(
            contents->GetUserData(
                &kTabDragControllerInteractiveUITestUserDataKey));
    if (user_data)
      result += base::IntToString(user_data->id());
    else
      result += "?";
  }
  return result;
}

// Creates a listener that quits the message loop when no longer dragging.
void QuitWhenNotDraggingImpl() {
  new QuitDraggingObserver();  // QuitDraggingObserver deletes itself.
}

TabStrip* GetTabStripForBrowser(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->tabstrip();
}

}  // namespace test

using ui_test_utils::GetCenterInScreenCoordinates;
using test::SetID;
using test::ResetIDs;
using test::IDString;
using test::GetTabStripForBrowser;

TabDragControllerTest::TabDragControllerTest()
    : browser_list(BrowserList::GetInstance()) {}

TabDragControllerTest::~TabDragControllerTest() {
}

void TabDragControllerTest::StopAnimating(TabStrip* tab_strip) {
  tab_strip->StopAnimating(true);
}

void TabDragControllerTest::AddTabAndResetBrowser(Browser* browser) {
  AddBlankTabAndShow(browser);
  StopAnimating(GetTabStripForBrowser(browser));
  ResetIDs(browser->tab_strip_model(), 0);
}

Browser* TabDragControllerTest::CreateAnotherWindowBrowserAndRelayout() {
  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ResetIDs(browser2->tab_strip_model(), 100);

  // Resize the two windows so they're right next to each other.
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .work_area();
  gfx::Size half_size =
      gfx::Size(work_area.width() / 3 - 10, work_area.height() / 2 - 10);
  browser()->window()->SetBounds(gfx::Rect(work_area.origin(), half_size));
  browser2->window()->SetBounds(gfx::Rect(
      work_area.x() + half_size.width(), work_area.y(),
      half_size.width(), half_size.height()));
  return browser2;
}

void TabDragControllerTest::SetWindowFinderForTabStrip(
    TabStrip* tab_strip,
    std::unique_ptr<WindowFinder> window_finder) {
  ASSERT_TRUE(tab_strip->drag_controller_.get());
  tab_strip->drag_controller_->window_finder_ = std::move(window_finder);
}

void TabDragControllerTest::HandleGestureEvent(TabStrip* tab_strip,
                                               ui::GestureEvent* event) {
  tab_strip->OnGestureEvent(event);
}

void TabDragControllerTest::SetUp() {
#if defined(USE_AURA)
  // This needs to be disabled as it can interfere with when events are
  // processed. In particular if input throttling is turned on, then when an
  // event ack runs the event may not have been processed.
  aura::Env::set_initial_throttle_input_on_resize_for_testing(false);
#endif
  InProcessBrowserTest::SetUp();
}

namespace {

enum InputSource {
  INPUT_SOURCE_MOUSE = 0,
  INPUT_SOURCE_TOUCH = 1
};

int GetDetachY(TabStrip* tab_strip) {
  return std::max(TabDragController::kTouchVerticalDetachMagnetism,
                  TabDragController::kVerticalDetachMagnetism) +
      tab_strip->height() + 1;
}

bool GetIsDragged(Browser* browser) {
#if defined(OS_CHROMEOS)
  return ash::wm::GetWindowState(browser->window()->GetNativeWindow())->
      is_dragged();
#else
  return false;
#endif
}

}  // namespace

#if defined(OS_CHROMEOS)
class ScreenEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  explicit ScreenEventGeneratorDelegate(aura::Window* root_window)
      : root_window_(root_window) {}
  ~ScreenEventGeneratorDelegate() override {}

  // EventGeneratorDelegateAura overrides:
  ui::EventTarget* GetTargetAt(const gfx::Point& point) override {
    return root_window_->GetHost()->window();
  }

  aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const override {
    return aura::client::GetScreenPositionClient(root_window_);
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ScreenEventGeneratorDelegate);
};

#endif

#if !defined(OS_CHROMEOS) && defined(USE_AURA)

// Following classes verify a crash scenario. Specifically on Windows when focus
// changes it can trigger capture being lost. This was causing a crash in tab
// dragging as it wasn't set up to handle this scenario. These classes
// synthesize this scenario.

// Allows making ClearNativeFocus() invoke ReleaseCapture().
class TestDesktopBrowserFrameAura : public DesktopBrowserFrameAura {
 public:
  TestDesktopBrowserFrameAura(
      BrowserFrame* browser_frame,
      BrowserView* browser_view)
      : DesktopBrowserFrameAura(browser_frame, browser_view),
        release_capture_(false) {}
  ~TestDesktopBrowserFrameAura() override {}

  void ReleaseCaptureOnNextClear() {
    release_capture_ = true;
  }

  void ClearNativeFocus() override {
    views::DesktopNativeWidgetAura::ClearNativeFocus();
    if (release_capture_) {
      release_capture_ = false;
      GetWidget()->ReleaseCapture();
    }
  }

 private:
  // If true ReleaseCapture() is invoked in ClearNativeFocus().
  bool release_capture_;

  DISALLOW_COPY_AND_ASSIGN(TestDesktopBrowserFrameAura);
};

// Factory for creating a TestDesktopBrowserFrameAura.
class TestNativeBrowserFrameFactory : public NativeBrowserFrameFactory {
 public:
  TestNativeBrowserFrameFactory() {}
  ~TestNativeBrowserFrameFactory() override {}

  NativeBrowserFrame* Create(BrowserFrame* browser_frame,
                             BrowserView* browser_view) override {
    return new TestDesktopBrowserFrameAura(browser_frame, browser_view);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNativeBrowserFrameFactory);
};

class TabDragCaptureLostTest : public TabDragControllerTest {
 public:
  TabDragCaptureLostTest() {
    NativeBrowserFrameFactory::Set(new TestNativeBrowserFrameFactory);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabDragCaptureLostTest);
};

// See description above for details.
IN_PROC_BROWSER_TEST_F(TabDragCaptureLostTest, ReleaseCaptureOnDrag) {
  AddTabAndResetBrowser(browser());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_1_center) &&
              ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  TestDesktopBrowserFrameAura* frame =
      static_cast<TestDesktopBrowserFrameAura*>(
          BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->
          native_widget_private());
  // Invoke ReleaseCaptureOnDrag() so that when the drag happens and focus
  // changes capture is released and the drag cancels.
  frame->ReleaseCaptureOnNextClear();
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  EXPECT_FALSE(tab_strip->IsDragSessionActive());
}

IN_PROC_BROWSER_TEST_F(TabDragControllerTest, GestureEndShouldEndDragTest) {
  AddTabAndResetBrowser(browser());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  Tab* tab1 = tab_strip->tab_at(1);
  gfx::Point tab_1_center(tab1->width() / 2, tab1->height() / 2);

  ui::GestureEvent gesture_tap_down(
      tab_1_center.x(), tab_1_center.x(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  tab_strip->MaybeStartDrag(tab1, gesture_tap_down,
    tab_strip->GetSelectionModel());
  EXPECT_TRUE(TabDragController::IsActive());

  ui::GestureEvent gesture_end(tab_1_center.x(), tab_1_center.x(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_END));
  HandleGestureEvent(tab_strip, &gesture_end);
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->IsDragSessionActive());
}

#endif

class DetachToBrowserTabDragControllerTest
    : public TabDragControllerTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  DetachToBrowserTabDragControllerTest() {}

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    root_ = browser()->window()->GetNativeWindow()->GetRootWindow();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(root_);
    // Disable flings which might otherwise inadvertently be generated from
    // tests' touch events.
    ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(
        std::numeric_limits<float>::max());
#endif
#if defined(OS_MACOSX)
    // Currently MacViews' browser windows are shown in the background and could
    // be obscured by other windows if there are any. This should be fixed in
    // order to be consistent with other platforms.
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#endif  // OS_MACOSX
  }

  InputSource input_source() const {
    return strstr(GetParam(), "mouse") ?
        INPUT_SOURCE_MOUSE : INPUT_SOURCE_TOUCH;
  }

#if defined(OS_CHROMEOS)
  // Converts the location in screen coordinate's to the location which event
  // generator expects (i.e. its root window's coordinate).
  gfx::Point GetLocationForEventGenerator(
      const gfx::Point& location_in_screen) {
    gfx::Point location_in_root = location_in_screen;
    aura::client::GetScreenPositionClient(root_)->ConvertPointFromScreen(
        root_, &location_in_root);
    return location_in_root;
  }
#endif

  // Set root window from a point in screen coordinates
  void SetEventGeneratorRootWindow(const gfx::Point& point) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return;
#if defined(OS_CHROMEOS)
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        std::make_unique<ScreenEventGeneratorDelegate>(
            ash::wm::GetRootWindowAt(point)));
#endif
  }

  // The following methods update one of the mouse or touch input depending upon
  // the InputSource.
  bool PressInput(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseMoveSync(location) &&
          ui_test_utils::SendMouseEventsSync(
              ui_controls::LEFT, ui_controls::DOWN);
    }
#if defined(OS_CHROMEOS)
    event_generator_->set_current_location(
        GetLocationForEventGenerator(location));
    event_generator_->PressTouch();
#else
    NOTREACHED();
#endif
    return true;
  }

  bool PressInput2() {
    // Second touch input is only used for touch sequence tests.
    EXPECT_EQ(INPUT_SOURCE_TOUCH, input_source());
#if defined(OS_CHROMEOS)
    event_generator_->set_current_location(
        event_generator_->current_location());
    event_generator_->PressTouchId(1);
#else
    NOTREACHED();
#endif
    return true;
  }

  bool DragInputTo(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_test_utils::SendMouseMoveSync(location);
#if defined(OS_CHROMEOS)
    event_generator_->MoveTouch(GetLocationForEventGenerator(location));
#else
    NOTREACHED();
#endif
    return true;
  }

  bool DragInputToAsync(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_controls::SendMouseMove(location.x(), location.y());
#if defined(OS_CHROMEOS)
    event_generator_->MoveTouch(GetLocationForEventGenerator(location));
#else
    NOTREACHED();
#endif
    return true;
  }

  bool DragInputToNotifyWhenDone(int x, int y, base::OnceClosure task) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_controls::SendMouseMoveNotifyWhenDone(x, y, std::move(task));
#if defined(OS_CHROMEOS)
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
    event_generator_->MoveTouch(GetLocationForEventGenerator(gfx::Point(x, y)));
#else
    NOTREACHED();
#endif
    return true;
  }

  bool DragInput2ToNotifyWhenDone(int x,
                                 int y,
                                 const base::Closure& task) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_controls::SendMouseMoveNotifyWhenDone(x, y, task);
#if defined(OS_CHROMEOS)
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
    event_generator_->MoveTouchId(
        GetLocationForEventGenerator(gfx::Point(x, y)), 1);
#else
    NOTREACHED();
#endif
    return true;
  }

  bool ReleaseInput() {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseEventsSync(
              ui_controls::LEFT, ui_controls::UP);
    }
#if defined(OS_CHROMEOS)
    event_generator_->ReleaseTouch();
#else
    NOTREACHED();
#endif
    return true;
  }

  bool ReleaseInput2() {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseEventsSync(
              ui_controls::LEFT, ui_controls::UP);
    }
#if defined(OS_CHROMEOS)
    event_generator_->ReleaseTouchId(1);
#else
    NOTREACHED();
#endif
    return true;
  }

  void ReleaseMouseAfterWindowDetached() {
    // On macOS, we want to avoid generating the input event [which requires an
    // associated window] until the window has been detached. Failure to do so
    // causes odd behavior [e.g. on macOS 10.10, the mouse-up will reactivate
    // the first window].
    if (browser_list->size() != 2u) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DetachToBrowserTabDragControllerTest::
                             ReleaseMouseAfterWindowDetached,
                         base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(1));
      return;
    }

    ASSERT_TRUE(ReleaseMouseAsync());
  }

  bool ReleaseMouseAsync() {
    return input_source() == INPUT_SOURCE_MOUSE &&
        ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP);
  }

  bool MoveInputTo(const gfx::Point& location) {
    aura::Env::GetInstance()->SetLastMouseLocation(location);
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_test_utils::SendMouseMoveSync(location);
#if defined(OS_CHROMEOS)
    event_generator_->set_current_location(
        GetLocationForEventGenerator(location));
#else
    NOTREACHED();
#endif
    return true;
  }

  void QuitWhenNotDragging() {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      // Schedule observer to quit message loop when done dragging. This has to
      // be async so the message loop can run.
      test::QuitWhenNotDraggingImpl();
      base::RunLoop().Run();
    } else {
      // Touch events are sync, so we know we're not in a drag session. But some
      // tests rely on the browser fully closing, which is async. So, run all
      // pending tasks.
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  void AddBlankTabAndShow(Browser* browser) {
    InProcessBrowserTest::AddBlankTabAndShow(browser);
  }

  // Returns true if the tab dragging info is correctly set on the attached
  // browser window. On Chrome OS, the info includes two window properties.
  bool IsTabDraggingInfoSet(TabStrip* attached_tabstrip,
                            TabStrip* source_tabstrip) {
    DCHECK(attached_tabstrip);
#if defined(OS_CHROMEOS)
    aura::Window* dragged_window =
        test::GetWindowForTabStrip(attached_tabstrip);
    attached_tabstrip->GetWidget()->GetNativeWindow();
    aura::Window* source_window =
        (source_tabstrip && source_tabstrip != attached_tabstrip)
            ? test::GetWindowForTabStrip(source_tabstrip)
            : nullptr;
    return dragged_window->GetProperty(ash::kIsDraggingTabsKey) &&
           dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey) ==
               source_window;
#else
    return true;
#endif
  }

  // Returns true if the tab dragging info is correctly cleared on the attached
  // browser window.
  bool IsTabDraggingInfoCleared(TabStrip* attached_tabstrip) {
    DCHECK(attached_tabstrip);
#if defined(OS_CHROMEOS)
    aura::Window* dragged_window =
        test::GetWindowForTabStrip(attached_tabstrip);
    return !dragged_window->GetProperty(ash::kIsDraggingTabsKey) &&
           !dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey);
#else
    return true;
#endif
  }

  Browser* browser() const { return InProcessBrowserTest::browser(); }

 private:
#if defined(OS_CHROMEOS)
  // The root window for the event generator.
  aura::Window* root_ = nullptr;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DetachToBrowserTabDragControllerTest);
};

// Creates a browser with two tabs, drags the second to the first.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest, DragInSameWindow) {
  // TODO(sky): this won't work with touch as it requires a long press.
  if (input_source() == INPUT_SOURCE_TOUCH) {
    VLOG(1) << "Test is DISABLED for touch input.";
    return;
  }

  AddTabAndResetBrowser(browser());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  ASSERT_TRUE(PressInput(tab_1_center));
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(DragInputTo(tab_0_center));
  // Test that the dragging info is correctly set on |tab_strip|.
  EXPECT_TRUE(IsTabDraggingInfoSet(tab_strip, tab_strip));
  ASSERT_TRUE(ReleaseInput());
  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->IsDragSessionActive());
  // Test that the dragging info is properly cleared after dragging.
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip));

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
}

#if defined(USE_AURA)
namespace {

// We need both MaskedWindowTargeter and MaskedWindowDelegate as they
// are used in two different pathes. crbug.com/493354.
class MaskedWindowTargeter : public aura::WindowTargeter {
 public:
  MaskedWindowTargeter() {}
  ~MaskedWindowTargeter() override {}

  // aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* target,
                                 const ui::LocatedEvent& event) const override {
    aura::Window* window = static_cast<aura::Window*>(target);
    gfx::Point local_point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);
    return window->GetEventHandlerForPoint(local_point);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaskedWindowTargeter);
};

}  // namespace

// The logic to find the target tabstrip should take the window mask into
// account. This test hangs without the fix. crbug.com/473080.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragWithMaskedWindows) {
  AddTabAndResetBrowser(browser());

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  const gfx::Rect bounds = browser_window->GetBoundsInScreen();
  aura::test::MaskedWindowDelegate masked_window_delegate(
      gfx::Rect(bounds.width() - 10, 0, 10, bounds.height()));
  gfx::Rect test(bounds);
  masked_window_delegate.set_can_focus(false);
  std::unique_ptr<aura::Window> masked_window(
      aura::test::CreateTestWindowWithDelegate(&masked_window_delegate, 10,
                                               test, browser_window->parent()));
  masked_window->SetEventTargeter(std::make_unique<MaskedWindowTargeter>());

  ASSERT_FALSE(masked_window->GetEventHandlerForPoint(
      gfx::Point(bounds.width() - 11, 0)));
  ASSERT_TRUE(masked_window->GetEventHandlerForPoint(
      gfx::Point(bounds.width() - 9, 0)));
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  ASSERT_TRUE(PressInput(tab_1_center));
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(DragInputTo(tab_0_center));
  ASSERT_TRUE(ReleaseInput());
  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->IsDragSessionActive());
}
#endif  // USE_AURA

namespace {

// Invoked from the nested run loop.
void DragToSeparateWindowStep2(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* not_attached_tab_strip,
                               TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Test that after the tabs are detached from the source tabstrip (in this
  // case |not_attached_tab_strip|), the tab dragging info should be properly
  // cleared on the source tabstrip.
  EXPECT_TRUE(test->IsTabDraggingInfoCleared(not_attached_tab_strip));
  // At this moment there should be a new browser window for the dragged tabs.
  EXPECT_EQ(3u, test->browser_list->size());
  Browser* new_browser = test->browser_list->get(2);
  TabStrip* new_tab_strip = GetTabStripForBrowser(new_browser);
  ASSERT_TRUE(new_tab_strip->IsDragSessionActive());
  // Test that the tab dragging info should be correctly set on the new window.
  EXPECT_TRUE(
      test->IsTabDraggingInfoSet(new_tab_strip, not_attached_tab_strip));

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() / 2,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Creates two browsers, drags from first into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragToSeparateWindowStep2,
                             this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip));
  EXPECT_TRUE(IsTabDraggingInfoSet(tab_strip2, tab_strip));

  // Release mouse or touch, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip2));
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
  EXPECT_FALSE(GetIsDragged(browser2));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
  EXPECT_FALSE(tab_strip2->GetWidget()->HasCapture());
}

namespace {

// WindowFinder that calls OnMouseCaptureLost() from
// GetLocalProcessWindowAtPoint().
class CaptureLoseWindowFinder : public WindowFinder {
 public:
  explicit CaptureLoseWindowFinder(TabStrip* tab_strip)
      : tab_strip_(tab_strip) {}
  ~CaptureLoseWindowFinder() override {}

  // WindowFinder:
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override {
    static_cast<views::View*>(tab_strip_)->OnMouseCaptureLost();
    return nullptr;
  }

 private:
  TabStrip* tab_strip_;

  DISALLOW_COPY_AND_ASSIGN(CaptureLoseWindowFinder);
};

}  // namespace

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_CaptureLostDuringDrag DISABLED_CaptureLostDuringDrag
#else
#define MAYBE_CaptureLostDuringDrag CaptureLostDuringDrag
#endif
// Calls OnMouseCaptureLost() from WindowFinder::GetLocalProcessWindowAtPoint()
// and verifies we don't crash. This simulates a crash seen on windows.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_CaptureLostDuringDrag) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Press on first tab so drag is active. Reset WindowFinder to one that causes
  // capture to be lost from within GetLocalProcessWindowAtPoint(), then
  // continue drag. The capture lost should trigger the drag to cancel.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  SetWindowFinderForTabStrip(
      tab_strip, base::WrapUnique(new CaptureLoseWindowFinder(tab_strip)));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
}

namespace {

void DetachToOwnWindowStep2(DetachToBrowserTabDragControllerTest* test) {
  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
}

#if defined(OS_CHROMEOS)
bool IsWindowPositionManaged(aura::Window* window) {
  return test::GetWindowForProperties(window)->GetProperty(
      ash::kWindowPositionManagedTypeKey);
}
bool HasUserChangedWindowPositionOrSize(aura::Window* window) {
  return ash::wm::GetWindowState(window)->bounds_changed_by_user();
}
#else
bool IsWindowPositionManaged(gfx::NativeWindow window) {
  return true;
}
bool HasUserChangedWindowPositionOrSize(gfx::NativeWindow window) {
  return false;
}
#endif

// Encapsulates waiting for the browser window to become maximized. This is
// needed for example on Chrome desktop linux, where window maximization is done
// asynchronously as an event received from a different process.
class MaximizedBrowserWindowWaiter {
 public:
  explicit MaximizedBrowserWindowWaiter(BrowserWindow* window)
      : window_(window) {}
  ~MaximizedBrowserWindowWaiter() = default;

  // Blocks until the browser window becomes maximized.
  void Wait() {
    if (CheckMaximized())
      return;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  bool CheckMaximized() {
    if (!window_->IsMaximized()) {
      base::MessageLoopCurrent::Get()->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(&MaximizedBrowserWindowWaiter::CheckMaximized),
              base::Unretained(this)));
      return false;
    }

    // Quit the run_loop to end the wait.
    if (!quit_.is_null())
      base::ResetAndReturn(&quit_).Run();
    return true;
  }

  // The browser window observed by this waiter.
  BrowserWindow* window_;

  // The waiter's RunLoop quit closure.
  base::Closure quit_;

  DISALLOW_COPY_AND_ASSIGN(MaximizedBrowserWindowWaiter);
};

}  // namespace

#if defined(OS_CHROMEOS)
#define MAYBE_DetachToOwnWindow DISABLED_DetachToOwnWindow
#else
#define MAYBE_DetachToOwnWindow DetachToOwnWindow
#endif
// Drags from browser to separate window and releases mouse.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DetachToOwnWindow) {
  const gfx::Rect initial_bounds(browser()->window()->GetBounds());
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DetachToOwnWindowStep2, this)));
  if (input_source() == INPUT_SOURCE_MOUSE) {
    ReleaseMouseAfterWindowDetached();
    QuitWhenNotDragging();
  }

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);

  bool check_new_window_active = true;
#if defined(OS_MACOSX)
  // AppKit 10.10 asynchronously reactivates the first
  // window. This behavior is non-deterministic, and appears to be a test-only
  // issue. Thus, we just skip the test check. https://crbug.com/862859.
  if (base::mac::IsOS10_10())
    check_new_window_active = false;
#endif
  if (check_new_window_active) {
    EXPECT_TRUE(new_browser->window()->IsActive());
  }
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  EXPECT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser()->window()->GetBounds().ToString());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(IsWindowPositionManaged(
      new_browser->window()->GetNativeWindow()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(new_browser->window()->IsMaximized());

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
  EXPECT_FALSE(tab_strip2->GetWidget()->HasCapture());
}

#if defined(OS_LINUX) || defined(OS_MACOSX)
// TODO(afakhry,varkha): Disabled on Linux as it fails on the bot because
// setting the window bounds to the work area bounds in
// DesktopWindowTreeHostX11::SetBounds() always insets it by one pixel in both
// width and height. This results in considering the source browser window not
// being full size, and the test is not as expected.
// crbug.com/626761, crbug.com/331924.
// TODO(tapted,mblsha): Disabled as the Mac IsMaximized() behavior is not
// consistent with other platforms. crbug.com/603562
#define MAYBE_DetachFromFullsizeWindow DISABLED_DetachFromFullsizeWindow
#else
#define MAYBE_DetachFromFullsizeWindow DetachFromFullsizeWindow
#endif
// Tests that a tab can be dragged from a browser window that is resized to full
// screen.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DetachFromFullsizeWindow) {
  // Resize the browser window so that it is as big as the work area.
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .work_area();
  browser()->window()->SetBounds(work_area);
  const gfx::Rect initial_bounds(browser()->window()->GetBounds());
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DetachToOwnWindowStep2, this)));
  if (input_source() == INPUT_SOURCE_MOUSE) {
    ReleaseMouseAfterWindowDetached();
    QuitWhenNotDragging();
  }

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser()->window()->GetBounds().ToString());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(
      IsWindowPositionManaged(new_browser->window()->GetNativeWindow()));

  // Only second window should be maximized.
  EXPECT_FALSE(browser()->window()->IsMaximized());
  MaximizedBrowserWindowWaiter(new_browser->window()).Wait();
  EXPECT_TRUE(new_browser->window()->IsMaximized());

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
  EXPECT_FALSE(tab_strip2->GetWidget()->HasCapture());
}

#if defined(OS_MACOSX)
// TODO(tapted,mblsha): Disabled as the Mac IsMaximized() behavior is not
// consistent with other platforms. crbug.com/603562
#define MAYBE_DetachToOwnWindowFromMaximizedWindow \
  DISABLED_DetachToOwnWindowFromMaximizedWindow
#else
#define MAYBE_DetachToOwnWindowFromMaximizedWindow \
  DetachToOwnWindowFromMaximizedWindow
#endif
// Drags from browser to a separate window and releases mouse.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DetachToOwnWindowFromMaximizedWindow) {
  // Maximize the initial browser window.
  browser()->window()->Maximize();
  MaximizedBrowserWindowWaiter(browser()->window()).Wait();
  ASSERT_TRUE(browser()->window()->IsMaximized());

  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DetachToOwnWindowStep2, this)));
  if (input_source() == INPUT_SOURCE_MOUSE) {
    ReleaseMouseAfterWindowDetached();
    QuitWhenNotDragging();
  }

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_TRUE(browser()->window()->IsMaximized());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(IsWindowPositionManaged(
      new_browser->window()->GetNativeWindow()));

  // The new window should be maximized.
  MaximizedBrowserWindowWaiter(new_browser->window()).Wait();
  EXPECT_TRUE(new_browser->window()->IsMaximized());
}

#if defined(OS_CHROMEOS)

// This test makes sense only on Chrome OS where we have the immersive
// fullscreen mode. The detached tab to a new browser window should remain in
// immersive fullscreen mode.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindowWhileInImmersiveFullscreenMode) {
  // Toggle the immersive fullscreen mode for the initial browser.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->immersive_mode_controller()
                  ->IsEnabled());

  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DetachToOwnWindowStep2, this)));
  if (input_source() == INPUT_SOURCE_MOUSE) {
    ReleaseMouseAfterWindowDetached();
    QuitWhenNotDragging();
  }

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->immersive_mode_controller()
                  ->IsEnabled());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(
      IsWindowPositionManaged(new_browser->window()->GetNativeWindow()));

  // The new browser should be in immersive fullscreen mode.
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(new_browser)
                  ->immersive_mode_controller()
                  ->IsEnabled());
  EXPECT_TRUE(new_browser->window()->IsFullscreen());
}

#endif

// Deletes a tab being dragged before the user moved enough to start a drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteBeforeStartedDragging) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  browser()->tab_strip_model()->DetachWebContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
  EXPECT_FALSE(GetIsDragged(browser()));
}

#if defined(OS_CHROMEOS)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_DeleteTabWhileAttached DISABLED_DeleteTabWhileAttached
#else
#define MAYBE_DeleteTabWhileAttached DeleteTabWhileAttached
#endif
// Deletes a tab being dragged while still attached.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DeleteTabWhileAttached) {
  // TODO(sky,sad): Disabled as it fails due to resize locks with a real
  // compositor. crbug.com/331924
  if (input_source() == INPUT_SOURCE_MOUSE) {
    VLOG(1) << "Test is DISABLED for mouse input.";
    return;
  }

  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab and move it enough so that it starts dragging but is
  // still attached.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputTo(
                  gfx::Point(tab_0_center.x() + 20, tab_0_center.y())));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  browser()->tab_strip_model()->DetachWebContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));
}

namespace {

void CloseTabsWhileDetachedStep2(const BrowserList* browser_list) {
  ASSERT_EQ(2u, browser_list->size());
  Browser* old_browser = browser_list->get(0);
  EXPECT_EQ("0 3", IDString(old_browser->tab_strip_model()));
  Browser* new_browser = browser_list->get(1);
  EXPECT_EQ("1 2", IDString(new_browser->tab_strip_model()));
  chrome::CloseTab(new_browser);
}

}  // namespace

#if defined(OS_CHROMEOS)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_DeleteTabsWhileDetached DISABLED_DeleteTabsWhileDetached
#else
#define MAYBE_DeleteTabsWhileDetached DeleteTabsWhileDetached
#endif
// Selects 2 tabs out of 4, drags them out and closes the new browser window
// while dragging tabs.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DeleteTabsWhileDetached) {
  // Add 3 tabs for a total of 4 tabs.
  AddTabAndResetBrowser(browser());
  AddTabAndResetBrowser(browser());
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  EXPECT_EQ("0 1 2 3", IDString(browser()->tab_strip_model()));

  // Click the first tab and select two middle tabs.
  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  gfx::Point tab_2_center(GetCenterInScreenCoordinates(tab_strip->tab_at(2)));
  ASSERT_TRUE(PressInput(tab_1_center));
  ASSERT_TRUE(ReleaseInput());
  browser()->tab_strip_model()->ToggleSelectionAt(2);
  // Press mouse button in the second tab and drag it enough to detach.
  ASSERT_TRUE(PressInput(tab_2_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_2_center.x(), tab_2_center.y() + GetDetachY(tab_strip),
      base::Bind(&CloseTabsWhileDetachedStep2, browser_list)));
  QuitWhenNotDragging();

  // Should not be dragging.
  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // Both tabs "1" and "2" get closed.
  EXPECT_EQ("0 3", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));
}

namespace {

void PressEscapeWhileDetachedStep2(const BrowserList* browser_list) {
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ui_controls::SendKeyPress(
      new_browser->window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
      false, false);
}

}  // namespace

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_PressEscapeWhileDetached DISABLED_PressEscapeWhileDetached
#else
#define MAYBE_PressEscapeWhileDetached PressEscapeWhileDetached
#endif
// This is disabled until NativeViewHost::Detach really detaches.
// Detaches a tab and while detached presses escape to revert the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_PressEscapeWhileDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&PressEscapeWhileDetachedStep2, browser_list)));
  QuitWhenNotDragging();

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, browser_list->size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
}

namespace {

void DragAllStep2(DetachToBrowserTabDragControllerTest* test,
                  const BrowserList* browser_list) {
  // Should only be one window.
  ASSERT_EQ(1u, browser_list->size());
  if (test->input_source() == INPUT_SOURCE_TOUCH) {
    ASSERT_TRUE(test->ReleaseInput());
  } else {
    ASSERT_TRUE(test->ReleaseMouseAsync());
  }
}

}  // namespace

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_DragAll DISABLED_DragAll
#else
#define MAYBE_DragAll DragAll
#endif
// Selects multiple tabs and starts dragging the window.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest, MAYBE_DragAll) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  browser()->tab_strip_model()->ToggleSelectionAt(0);
  const gfx::Rect initial_bounds = browser()->window()->GetBounds();

  // Move to the first tab and drag it enough so that it would normally
  // detach.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragAllStep2, this, browser_list)));
  QuitWhenNotDragging();

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, browser_list->size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());

  const gfx::Rect final_bounds = browser()->window()->GetBounds();
  // Size unchanged, but it should have moved down.
  EXPECT_EQ(initial_bounds.size(), final_bounds.size());
  EXPECT_EQ(initial_bounds.origin().x(), final_bounds.origin().x());
  EXPECT_EQ(initial_bounds.origin().y() + GetDetachY(tab_strip),
            final_bounds.origin().y());
}

namespace {

// Invoked from the nested run loop.
void DragAllToSeparateWindowStep2(DetachToBrowserTabDragControllerTest* test,
                                  TabStrip* attached_tab_strip,
                                  TabStrip* target_tab_strip,
                                  const BrowserList* browser_list) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() / 2,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_DragAllToSeparateWindow DISABLED_DragAllToSeparateWindow
#else
#define MAYBE_DragAllToSeparateWindow DragAllToSeparateWindow
#endif
// Creates two browsers, selects all tabs in first and drags into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragAllToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->ToggleSelectionAt(0);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragAllToSeparateWindowStep2, this, tab_strip, tab_strip2,
                 browser_list)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

namespace {

// Invoked from the nested run loop.
void DragAllToSeparateWindowAndCancelStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* attached_tab_strip,
    TabStrip* target_tab_strip,
    const BrowserList* browser_list) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() / 2,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_DragAllToSeparateWindowAndCancel \
  DISABLED_DragAllToSeparateWindowAndCancel
#else
#define MAYBE_DragAllToSeparateWindowAndCancel DragAllToSeparateWindowAndCancel
#endif
// Creates two browsers, selects all tabs in first, drags into second, then hits
// escape.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragAllToSeparateWindowAndCancel) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->ToggleSelectionAt(0);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragAllToSeparateWindowAndCancelStep2, this,
                             tab_strip, tab_strip2, browser_list)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Cancel the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser2, ui::VKEY_ESCAPE, false, false, false, false));

  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  // browser() will have been destroyed, but browser2 should remain.
  ASSERT_EQ(1u, browser_list->size());

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
// Also fails on mac, crbug.com/837219
// Creates two browsers, drags from first into the second in such a way that
// no detaching should happen.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DISABLED_DragDirectlyToSecondWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move the tabstrip down enough so that we can detach.
  gfx::Rect bounds(browser2->window()->GetBounds());
  bounds.Offset(0, 100);
  browser2->window()->SetBounds(bounds);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  gfx::Point b2_location(5, 0);
  views::View::ConvertPointToScreen(tab_strip2, &b2_location);
  ASSERT_TRUE(DragInputTo(b2_location));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(browser2));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_DragSingleTabToSeparateWindow \
  DISABLED_DragSingleTabToSeparateWindow
#else
#define MAYBE_DragSingleTabToSeparateWindow DragSingleTabToSeparateWindow
#endif
// Creates two browsers, the first browser has a single tab and drags into the
// second browser.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragSingleTabToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  ResetIDs(browser()->tab_strip_model(), 0);

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  const gfx::Rect initial_bounds(browser2->window()->GetBounds());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragAllToSeparateWindowStep2, this, tab_strip, tab_strip2,
                 browser_list)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());

  // Make sure that the window is still managed and not user moved.
  EXPECT_TRUE(IsWindowPositionManaged(browser2->window()->GetNativeWindow()));
  EXPECT_FALSE(HasUserChangedWindowPositionOrSize(
      browser2->window()->GetNativeWindow()));
  // Also make sure that the drag to window position has not changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser2->window()->GetBounds().ToString());
}

namespace {

// Invoked from the nested run loop.
void CancelOnNewTabWhenDraggingStep2(
    DetachToBrowserTabDragControllerTest* test,
    const BrowserList* browser_list) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  chrome::AddTabAt(browser_list->GetLastActive(), GURL(url::kAboutBlankURL),
                   0, false);
}

}  // namespace

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(sky,sad): Disabled as it fails due to resize locks with a real
// compositor. crbug.com/331924
#define MAYBE_CancelOnNewTabWhenDragging DISABLED_CancelOnNewTabWhenDragging
#else
#define MAYBE_CancelOnNewTabWhenDragging CancelOnNewTabWhenDragging
#endif
// Adds another tab, detaches into separate window, adds another tab and
// verifies the run loop ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_CancelOnNewTabWhenDragging) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  // Add another tab. This should trigger exiting the nested loop. Add at the
  // beginning to exercise past crash when model/tabstrip got out of sync.
  // crbug.com/474082
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&CancelOnNewTabWhenDraggingStep2, this, browser_list)));
  observer.Wait();

  // Should be two windows and not dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());
  for (auto* browser : *BrowserList::GetInstance()) {
    EXPECT_FALSE(GetIsDragged(browser));
    // Should not be maximized
    EXPECT_FALSE(browser->window()->IsMaximized());
  }
}

#if defined(OS_CHROMEOS)
// TODO(sky,sad): A number of tests below are disabled as they fail due to
// resize locks with a real compositor. crbug.com/331924
namespace {

void DragInMaximizedWindowStep2(DetachToBrowserTabDragControllerTest* test,
                                Browser* browser,
                                TabStrip* tab_strip,
                                const BrowserList* browser_list) {
  // There should be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  EXPECT_NE(browser, new_browser);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);

  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());

  // Both windows should be visible.
  EXPECT_TRUE(tab_strip->GetWidget()->IsVisible());
  EXPECT_TRUE(tab_strip2->GetWidget()->IsVisible());

  // Stops dragging.
  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Creates a browser with two tabs, maximizes it, drags the tab out.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DISABLED_DragInMaximizedWindow) {
  AddTabAndResetBrowser(browser());
  browser()->window()->Maximize();

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragInMaximizedWindowStep2, this, browser(), tab_strip,
                 browser_list)));
  QuitWhenNotDragging();

  ASSERT_FALSE(TabDragController::IsActive());

  // Should be two browsers.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());

  EXPECT_TRUE(browser()->window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(new_browser->window()->GetNativeWindow()->IsVisible());

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));

  // The source window should be maximized.
  EXPECT_TRUE(browser()->window()->IsMaximized());
  // The new window should be maximized.
  EXPECT_TRUE(new_browser->window()->IsMaximized());
}

// Subclass of DetachToBrowserTabDragControllerTest that
// creates multiple displays.
class DetachToBrowserInSeparateDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayTabDragControllerTest() {}
  virtual ~DetachToBrowserInSeparateDisplayTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-600x600,601+0-600x600");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DetachToBrowserInSeparateDisplayTabDragControllerTest);
};

// Subclass of DetachToBrowserTabDragControllerTest that runs tests only with
// touch input.
class DetachToBrowserTabDragControllerTestTouch
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserTabDragControllerTestTouch() {}
  virtual ~DetachToBrowserTabDragControllerTestTouch() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DetachToBrowserTabDragControllerTestTouch);
};

namespace {

void DragSingleTabToSeparateWindowInSecondDisplayStep3(
    DetachToBrowserTabDragControllerTest* test) {
  ASSERT_TRUE(test->ReleaseInput());
}

void DragSingleTabToSeparateWindowInSecondDisplayStep2(
    DetachToBrowserTabDragControllerTest* test,
    const gfx::Point& target_point) {
  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      target_point.x(), target_point.y(),
      base::Bind(&DragSingleTabToSeparateWindowInSecondDisplayStep3, test)));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragSingleTabToSeparateWindowInSecondDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  // Then drag it to the final destination on the second screen.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragSingleTabToSeparateWindowInSecondDisplayStep2,
                             this, gfx::Point(600 + tab_0_center.x(),
                                              tab_0_center.y()
                                              + GetDetachY(tab_strip)))));
  QuitWhenNotDragging();

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  // This other browser should be on the second screen (with mouse drag)
  // With the touch input the browser cannot be dragged from one screen
  // to another and the window stays on the first screen.
  if (input_source() == INPUT_SOURCE_MOUSE) {
    display::Screen* screen = display::Screen::GetScreen();
    EXPECT_EQ(
        ui_test_utils::GetSecondaryDisplay(screen).id(),
        screen
            ->GetDisplayNearestWindow(new_browser->window()->GetNativeWindow())
            .id());
  }

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(new_browser->window()->IsMaximized());
}

namespace {

// Invoked from the nested run loop.
void DragTabToWindowInSeparateDisplayStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(
      GetCenterInScreenCoordinates(target_tab_strip->tab_at(0)));

  // Move it closer to the beginning of the tab so it will drop before that tab.
  target_point.Offset(-20, 0);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Drags from browser to another browser on a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragTabToWindowInSeparateDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move the second browser to the second display.
  display::Screen* screen = display::Screen::GetScreen();
  Display second_display = ui_test_utils::GetSecondaryDisplay(screen);
  browser2->window()->SetBounds(second_display.work_area());
  // In Mash, the display change as the result of the bounds change is processed
  // asynchronously in the window server, it should wait for those changes to
  // complete.
  aura::test::WaitForAllChangesToComplete();
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragTabToWindowInSeparateDisplayStep2,
                             this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragBrowserWindowWhenMajorityOfBoundsInSecondDisplay) {
  // Set the browser's window bounds such that the majority of its bounds
  // resides in the second display.
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(2, screen->GetNumDisplays());
  const std::pair<Display, Display> displays = GetDisplays(screen);
  gfx::Rect browser_bounds =
      browser()->window()->GetNativeWindow()->GetBoundsInScreen();
  browser_bounds.set_x(displays.first.bounds().right() -
                       (browser_bounds.width() / 2) + 20);
  browser()->window()->GetNativeWindow()->SetBounds(browser_bounds);
  EXPECT_EQ(
      displays.first.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());

  // Start dragging the window by the tab strip, and move it only to the edge
  // of the first display. Expect at that point mouse would warp and the window
  // will therefore reside in the second display when mouse is released.
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  gfx::Point target_point = tab_0_center;
  target_point.Offset(0, GetDetachY(tab_strip));
  const int first_display_warp_edge_x = displays.first.bounds().right() - 1;
  target_point.set_x(first_display_warp_edge_x);

  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragSingleTabToSeparateWindowInSecondDisplayStep2, this,
                 target_point)));

  QuitWhenNotDragging();

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should only be a single browser.
  ASSERT_EQ(1u, browser_list->size());
  ASSERT_EQ(browser(), browser_list->get(0));
  ASSERT_TRUE(browser()->window()->IsActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());

  // Browser now resides in display 2.
  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());
}

// Drags from browser to another browser on a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragTabToWindowOnSecondDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move both browsers to be side by side on the second display.
  display::Screen* screen = display::Screen::GetScreen();
  Display second_display = ui_test_utils::GetSecondaryDisplay(screen);
  gfx::Rect work_area = second_display.work_area();
  work_area.set_width(work_area.width() / 2);
  browser()->window()->SetBounds(work_area);
  work_area.set_x(work_area.right());
  browser2->window()->SetBounds(work_area);
  // Wait for the display changes. See the ealier comments for the details.
  aura::test::WaitForAllChangesToComplete();
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  // SetEventGeneratorRootWindow sets correct (second) RootWindow
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  SetEventGeneratorRootWindow(tab_0_center);
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragTabToWindowInSeparateDisplayStep2,
                             this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// Drags from a maximized browser to another non-maximized browser on a second
// display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragMaxTabToNonMaxWindowInSeparateDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  browser()->window()->Maximize();
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser on the second display.
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(2, screen->GetNumDisplays());
  const std::pair<Display, Display> displays = GetDisplays(screen);
  gfx::Rect work_area = displays.second.work_area();
  work_area.Inset(20, 20, 20, 60);
  Browser::CreateParams params(browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_NORMAL;
  params.initial_bounds = work_area;
  Browser* browser2 = new Browser(params);
  AddBlankTabAndShow(browser2);

  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());
  EXPECT_EQ(
      displays.first.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());
  EXPECT_EQ(2, tab_strip->tab_count());
  EXPECT_EQ(1, tab_strip2->tab_count());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragTabToWindowInSeparateDisplayStep2,
                             this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());

  // tab should have moved
  EXPECT_EQ(1, tab_strip->tab_count());
  EXPECT_EQ(2, tab_strip2->tab_count());

  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Source browser should still be maximized, target should not
  EXPECT_TRUE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// Drags from a restored browser to an immersive fullscreen browser on a
// second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DISABLED_DragTabToImmersiveBrowserOnSeparateDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move the second browser to the second display.
  display::Screen* screen = display::Screen::GetScreen();
  const std::pair<Display, Display> displays = GetDisplays(screen);
  browser2->window()->SetBounds(displays.second.work_area());
  // Wait for the display changes. See the ealier comments for the details.
  aura::test::WaitForAllChangesToComplete();
  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Put the second browser into immersive fullscreen.
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  ImmersiveModeController* immersive_controller2 =
      browser_view2->immersive_mode_controller();
  ASSERT_EQ(ImmersiveModeController::Type::ASH, immersive_controller2->type());
  ash::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerAsh*>(immersive_controller2)
          ->controller())
      .SetupForTest();
  chrome::ToggleFullscreenMode(browser2);
  // For MD, the browser's top chrome is completely offscreen, with tabstrip
  // visible.
  ASSERT_TRUE(immersive_controller2->IsEnabled());
  ASSERT_FALSE(immersive_controller2->IsRevealed());
  ASSERT_TRUE(tab_strip2->visible());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragTabToWindowInSeparateDisplayStep2,
                             this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // browser2's top chrome should be revealed and the tab strip should be
  // at normal height while user is tragging tabs_strip2's tabs.
  ASSERT_TRUE(immersive_controller2->IsRevealed());
  ASSERT_TRUE(tab_strip2->visible());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Move the mouse off of browser2's top chrome.
  ASSERT_TRUE(
      ui_test_utils::SendMouseMoveSync(displays.first.bounds().CenterPoint()));

  // The first browser window should not be in immersive fullscreen.
  // browser2 should still be in immersive fullscreen, but the top chrome should
  // no longer be revealed.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());

  EXPECT_TRUE(immersive_controller2->IsEnabled());
  EXPECT_FALSE(immersive_controller2->IsRevealed());
  EXPECT_TRUE(tab_strip2->visible());
}

// Subclass of DetachToBrowserTabDragControllerTest that
// creates multiple displays with different device scale factors.
class DifferentDeviceScaleFactorDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DifferentDeviceScaleFactorDisplayTabDragControllerTest() {}
  virtual ~DifferentDeviceScaleFactorDisplayTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "400x400,0+400-800x800*2");
  }

  float GetCursorDeviceScaleFactor() const {
    ash::CursorManagerTestApi cursor_test_api(
        ash::Shell::Get()->cursor_manager());
    return cursor_test_api.GetCurrentCursor().device_scale_factor();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DifferentDeviceScaleFactorDisplayTabDragControllerTest);
};

namespace {

// The points where a tab is dragged in CursorDeviceScaleFactorStep.
const struct DragPoint {
  int x;
  int y;
} kDragPoints[] = {
  {300, 200},
  {399, 200},
  {500, 200},
  {400, 200},
  {300, 200},
};

// The expected device scale factors before the cursor is moved to the
// corresponding kDragPoints in CursorDeviceScaleFactorStep.
const float kDeviceScaleFactorExpectations[] = {
  1.0f,
  1.0f,
  2.0f,
  2.0f,
  1.0f,
};

static_assert(
    arraysize(kDragPoints) == arraysize(kDeviceScaleFactorExpectations),
    "kDragPoints and kDeviceScaleFactorExpectations must have the same "
    "number of elements");

// Drags tab to |kDragPoints[index]|, then calls the next step function.
void CursorDeviceScaleFactorStep(
    DifferentDeviceScaleFactorDisplayTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    size_t index) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  if (index < arraysize(kDragPoints)) {
    EXPECT_EQ(kDeviceScaleFactorExpectations[index],
              test->GetCursorDeviceScaleFactor());
    const DragPoint p = kDragPoints[index];
    ASSERT_TRUE(test->DragInputToNotifyWhenDone(
        p.x, p.y, base::Bind(&CursorDeviceScaleFactorStep,
                             test, not_attached_tab_strip, index + 1)));
  } else {
    // Finishes a serise of CursorDeviceScaleFactorStep calls and ends drag.
    EXPECT_EQ(1.0f, test->GetCursorDeviceScaleFactor());
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
        ui_controls::LEFT, ui_controls::UP));
  }
}

}  // namespace

// Verifies cursor's device scale factor is updated when a tab is moved across
// displays with different device scale factors (http://crbug.com/154183).
IN_PROC_BROWSER_TEST_P(DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                       DISABLED_CursorDeviceScaleFactor) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move the second browser to the second display.
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&CursorDeviceScaleFactorStep,
                             this, tab_strip, 0)));
  QuitWhenNotDragging();
}

namespace {

class DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest
    : public TabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-250x250,251+0-250x250");
  }

  bool Press(const gfx::Point& position) {
    return ui_test_utils::SendMouseMoveSync(position) &&
        ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                           ui_controls::DOWN);
  }

  bool DragTabAndExecuteTaskWhenDone(const gfx::Point& position,
                                     base::OnceClosure task) {
    return ui_controls::SendMouseMoveNotifyWhenDone(position.x(), position.y(),
                                                    std::move(task));
  }

  void QuitWhenNotDragging() {
    DCHECK(TabDragController::IsActive());
    test::QuitWhenNotDraggingImpl();
    base::RunLoop().Run();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest);
};

// Invoked from the nested run loop.
void CancelDragTabToWindowInSeparateDisplayStep3(
    TabStrip* tab_strip,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Switching display mode should cancel the drag operation.
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display_manager->AddRemoveDisplay();
}

// Invoked from the nested run loop.
void CancelDragTabToWindowInSeparateDisplayStep2(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest* test,
    TabStrip* tab_strip,
    Display current_display,
    gfx::Point final_destination,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  Browser* new_browser = browser_list->get(1);
  EXPECT_EQ(
      current_display.id(),
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(new_browser->window()->GetNativeWindow())
          .id());

  ASSERT_TRUE(test->DragTabAndExecuteTaskWhenDone(
      final_destination,
      base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep3, tab_strip,
                     browser_list)));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_F(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn2ndDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Move the second browser to the second display.
  const std::pair<Display, Display> displays =
      GetDisplays(display::Screen::GetScreen());
  gfx::Point final_destination = displays.second.work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  gfx::Point tab_0_dst(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(Press(tab_0_dst));
  tab_0_dst.Offset(0, GetDetachY(tab_strip));
  ASSERT_TRUE(DragTabAndExecuteTaskWhenDone(
      tab_0_dst, base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep2,
                                this, tab_strip, displays.first,
                                final_destination, browser_list)));
  QuitWhenNotDragging();

  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::UP));
}

// Drags from browser from a second display to primary and releases input.
IN_PROC_BROWSER_TEST_F(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn1stDisplay) {
  display::Screen* screen = display::Screen::GetScreen();
  const std::pair<Display, Display> displays = GetDisplays(screen);

  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
  EXPECT_EQ(
      displays.first.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());

  browser()->window()->SetBounds(displays.second.work_area());
  // Wait for the display changes. See the ealier comments for the details.
  aura::test::WaitForAllChangesToComplete();
  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());

  // Move the second browser to the display.
  gfx::Point final_destination = displays.first.work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  gfx::Point tab_0_dst(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(Press(tab_0_dst));
  tab_0_dst.Offset(0, GetDetachY(tab_strip));
  ASSERT_TRUE(DragTabAndExecuteTaskWhenDone(
      tab_0_dst, base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep2,
                                this, tab_strip, displays.second,
                                final_destination, browser_list)));
  QuitWhenNotDragging();

  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::UP));
}

namespace {
void PressSecondFingerWhileDetachedStep3(
    DetachToBrowserTabDragControllerTest* test) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());
  ASSERT_TRUE(test->browser_list->get(1)->window()->IsActive());

  ASSERT_TRUE(test->ReleaseInput());
  ASSERT_TRUE(test->ReleaseInput2());
}

void PressSecondFingerWhileDetachedStep2(
    DetachToBrowserTabDragControllerTest* test,
    const gfx::Point& target_point) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());
  ASSERT_TRUE(test->browser_list->get(1)->window()->IsActive());

  // Continue dragging after adding a second finger.
  ASSERT_TRUE(test->PressInput2());
  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      target_point.x(), target_point.y(),
      base::Bind(&PressSecondFingerWhileDetachedStep3, test)));
}

}  // namespace

// Detaches a tab and while detached presses a second finger.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       PressSecondFingerWhileDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&PressSecondFingerWhileDetachedStep2, this,
                 gfx::Point(tab_0_center.x(),
                            tab_0_center.y() + 2 * GetDetachY(tab_strip)))));
  QuitWhenNotDragging();

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

namespace {

void DragToOverviewWindowStep2(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* not_attached_tab_strip,
                               TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // And there should be three browser windows, incluing the newly created one
  // for the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Put the window that accociated with |target_tab_strip| in overview.
  test::GetWindowForTabStrip(target_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);

  // Drag to target_tab_strip.
  gfx::Point target_point(target_tab_strip->width() / 2,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputTo(target_point));

  // Test that the dragged tab did not attach to the overview window.
  EXPECT_EQ(3u, test->browser_list->size());

  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
  else
    ASSERT_TRUE(test->ReleaseMouseAsync());
}

}  // namespace

// Creates two browsers, drags from first into second. If the target window is
// currently showing in overview, we should not attaching the dragged tabs
// into the target window during dragging, but should do so until the drag ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToOverviewWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::BindOnce(&DragToOverviewWindowStep2, this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Now the dragged tab should have been attached to the target tabstrip after
  // the drag ends.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
}

namespace {

void DragToOverviewNewWindowItemStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* attached_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  EXPECT_TRUE(attached_tab_strip->GetWidget()->GetNativeWindow()->HasFocus());

  // Put the attached window in overview to simulate the "drop on the new
  // selector item" scenario.
  test::GetWindowForTabStrip(attached_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);
  // At the same time we remove |attached_tab_strip|'s focus to simulate what
  // happens in overview (In overview, the window items in overview don't have
  // focus, it's the textfield in overview that has focus).
  attached_tab_strip->GetFocusManager()->SetFocusedView(nullptr);

  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
  else
    ASSERT_TRUE(test->ReleaseMouseAsync());
}

}  // namespace

// After dragging a window to drop it onto the new window selector item in
// overview mode, the window should be added to overview window grid, and should
// not restore its focus.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToOverviewNewWindowItem) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Make |tab_strip| the focused view before dragging.
  tab_strip->GetFocusManager()->SetFocusedView(tab_strip);
  EXPECT_TRUE(tab_strip->HasFocus());

  // Drag the tab long enough so that it moves.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::BindOnce(&DragToOverviewNewWindowItemStep2, this, tab_strip)));
  QuitWhenNotDragging();

  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(1u, browser_list->size());

  // Test that the attached tabstrip doesn't restore focuas as it's currently
  // showing in overview.
  EXPECT_TRUE(test::GetWindowForTabStrip(tab_strip)->GetProperty(
      ash::kIsShowingInOverviewKey));
  EXPECT_FALSE(tab_strip->HasFocus());
}

namespace {

// A window observer that observes the dragged window's property
// ash::kIsDraggingTabsKey.
class DraggedWindowObserver : public aura::WindowObserver {
 public:
  DraggedWindowObserver(DetachToBrowserTabDragControllerTest* test,
                        aura::Window* window,
                        const gfx::Rect& bounds,
                        const gfx::Point& end_point)
      : test_(test), end_bounds_(bounds), end_point_(end_point) {}

  ~DraggedWindowObserver() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  void StartObserving(aura::Window* window) {
    DCHECK(!window_);
    window_ = window;
    window_->AddObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window_, window);
    if (key == ash::kIsDraggingTabsKey) {
      if (!window_->GetProperty(ash::kIsDraggingTabsKey)) {
        // It should be triggered by TabDragController::ClearTabDraggingInfo()
        // from TabDragController::EndDragImpl(). Theoretically at this point
        // TabDragController should have removed itself as an observer of the
        // dragged tabstrip's widget. So changing its bounds should do nothing.

        // It's to ensure the current cursor location is within the bounds of
        // another browser's tabstrip.
        test_->MoveInputTo(end_point_);

        // Change window's bounds to simulate what might happend in ash. If
        // TabDragController is still an observer of the dragged tabstrip's
        // widget, OnWidgetBoundsChanged() will calls into ContinueDragging()
        // to attach the dragged tabstrip into another browser, which might
        // cause chrome crash.
        window_->SetBounds(end_bounds_);
      }
    }
  }

 private:
  DetachToBrowserTabDragControllerTest* test_;
  // The dragged window.
  aura::Window* window_ = nullptr;
  // The bounds that |window_| will change to when the drag ends.
  gfx::Rect end_bounds_;
  // The position that the mouse/touch event will move to when the drag ends.
  gfx::Point end_point_;

  DISALLOW_COPY_AND_ASSIGN(DraggedWindowObserver);
};

void DoNotObserveDraggedWidgetAfterDragEndsStep2(
    DetachToBrowserTabDragControllerTest* test,
    DraggedWindowObserver* observer,
    TabStrip* attached_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Start observe the dragged window.
  observer->StartObserving(attached_tab_strip->GetWidget()->GetNativeWindow());

  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
  else
    ASSERT_TRUE(test->ReleaseMouseAsync());
}

}  // namespace

// Test that after the drag ends, TabDragController is no longer an observer of
// the dragged widget, so that if the bounds of the dragged widget change,
// TabDragController won't be put into dragging process again.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DoNotObserveDraggedWidgetAfterDragEnds) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  // Create an window observer to observe the dragged window.
  gfx::Point target_point(tab_strip2->bounds().CenterPoint());
  views::View::ConvertPointToScreen(tab_strip2, &target_point);
  std::unique_ptr<DraggedWindowObserver> observer(new DraggedWindowObserver(
      this, test::GetWindowForTabStrip(tab_strip),
      tab_strip2->GetWidget()->GetNativeWindow()->bounds(), target_point));

  // Drag the tab long enough so that it moves.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::BindOnce(&DoNotObserveDraggedWidgetAfterDragEndsStep2, this,
                     observer.get(), tab_strip)));
  QuitWhenNotDragging();

  // There should be still two browsers at this moment. |tab_strip| should not
  // be merged into |tab_strip2|.
  EXPECT_EQ(2u, browser_list->size());

  ASSERT_FALSE(TabDragController::IsActive());
}

namespace {

void DoNotAttachToOtherWindowTestStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    TabStrip* target_tab_strip) {
  ASSERT_TRUE(TabDragController::IsActive());

  // There should be three browser windows, incluing the newly created one for
  // the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());
  // Get this new created window and set it to non-attachable.
  Browser* new_browser = test->browser_list->get(2);
  test::GetWindowForProperties(new_browser->window()->GetNativeWindow())
      ->SetProperty(ash::kCanAttachToAnotherWindowKey, false);

  // Now drag to target_tab_strip.
  gfx::Point target_point(target_tab_strip->width() / 2,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputTo(target_point));

  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
  else
    ASSERT_TRUE(test->ReleaseMouseAsync());
}

}  // namespace

// Test that if the dragged window is not allowed to attach to another window
// during dragging, then it can't attach to another window.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DoNotAttachToOtherWindowTest) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::BindOnce(&DoNotAttachToOtherWindowTestStep2, this, tab_strip,
                     tab_strip2)));
  QuitWhenNotDragging();

  // Test that the newly created browser window doesn't attach to the target
  // browser window.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(3u, browser_list->size());
}

namespace {

void DeferredTargetTabStripTestStep2(DetachToBrowserTabDragControllerTest* test,
                                     TabStrip* not_attached_tab_strip,
                                     TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // And there should be three browser windows, incluing the newly created one
  // for the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Put the window that accociated with |target_tab_strip| in overview.
  test::GetWindowForTabStrip(target_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);

  // Drag to target_tab_strip.
  gfx::Point target_point(target_tab_strip->width() / 2,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputTo(target_point));

  // At this point, |target_tab_strip| should be the deferred target tabstip.
  // Theoratically the dragged tabstrip will merge into |target_tab_strip| after
  // the drag ends.
  EXPECT_TRUE(test::GetWindowForTabStrip(target_tab_strip)
                  ->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));

  // Now clear the property.
  test::GetWindowForTabStrip(target_tab_strip)
      ->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);

  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
  else
    ASSERT_TRUE(test->ReleaseMouseAsync());
}

}  // namespace

// Test that if a tabstrip is a deferred target tabstrip, and its corresponding
// window key is cleared to remove itself as the deferred target tabstrip, the
// dragged tabstrip should not attach into it after the drag ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeferredTargetTabStripTest) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::BindOnce(&DeferredTargetTabStripTestStep2, this, tab_strip,
                     tab_strip2)));
  QuitWhenNotDragging();

  // Now the dragged tab should not be attached to the target tabstrip after
  // the drag ends.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(3u, browser_list->size());
}

namespace {

void SecondFingerPressTestStep3(DetachToBrowserTabDragControllerTest* test) {
  ASSERT_TRUE(test->ReleaseInput());
}

void SecondFingerPressTestStep2(DetachToBrowserTabDragControllerTest* test,
                                TabStrip* not_attached_tab_strip,
                                TabStrip* target_tab_strip) {
  ASSERT_TRUE(TabDragController::IsActive());

  // And there should be three browser windows, including the newly created one
  // for the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Put the window that accociated with |target_tab_strip| in overview.
  test::GetWindowForTabStrip(target_tab_strip)
      ->SetProperty(ash::kIsShowingInOverviewKey, true);

  // Drag to |target_tab_strip|.
  gfx::Point target_point(target_tab_strip->width() / 2,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputTo(target_point));

  // Now add a second finger to tap on it.
  not_attached_tab_strip->GetWidget()->GetNativeWindow()->env()->set_touch_down(
      true);
  ASSERT_TRUE(test->PressInput2());
  ASSERT_TRUE(test->ReleaseInput2());

  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      target_point.x(), target_point.y(),
      base::BindOnce(&SecondFingerPressTestStep3, test)));
}

}  // namespace

// Tests that when drgging a tab to a browser window that's currently in
// overview, press the second finger should not cause chrome crash.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       SecondFingerPressTest) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::BindOnce(&SecondFingerPressTestStep2, this, tab_strip,
                     tab_strip2)));
  QuitWhenNotDragging();

  // Test that after dragging there is no crash and the dragged tab should now
  // be merged into the target tabstrip.
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
}

#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DetachToBrowserInSeparateDisplayTabDragControllerTest,
                        ::testing::Values("mouse"));
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                        ::testing::Values("mouse"));
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DetachToBrowserTabDragControllerTest,
                        ::testing::Values("mouse", "touch"));
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DetachToBrowserTabDragControllerTestTouch,
                        ::testing::Values("touch"));
#else
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DetachToBrowserTabDragControllerTest,
                        ::testing::Values("mouse"));
#endif
