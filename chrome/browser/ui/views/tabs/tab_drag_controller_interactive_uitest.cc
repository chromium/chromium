// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "ui/aura/env.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_targeter.h"
#endif

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/split_view_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_lacros.h"
#define DESKTOP_BROWSER_FRAME_AURA DesktopBrowserFrameLacros
#elif BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#define DESKTOP_BROWSER_FRAME_AURA DesktopBrowserFrameAuraLinux
#else
#define DESKTOP_BROWSER_FRAME_AURA DesktopBrowserFrameAura
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/ui_base_features.h"
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
  ~TabDragControllerInteractiveUITestUserData() override = default;
  int id() { return id_; }

 private:
  int id_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
aura::Window* GetWindowForTabStrip(TabStrip* tab_strip) {
  return tab_strip ? tab_strip->GetWidget()->GetNativeWindow() : nullptr;
}
#endif

gfx::Point GetLeftCenterInScreenCoordinates(const views::View* view) {
  gfx::Point center = view->GetLocalBounds().CenterPoint();
  center.set_x(center.x() - view->GetLocalBounds().width() / 4);
  views::View::ConvertPointToScreen(view, &center);
  return center;
}

gfx::Point GetRightCenterInScreenCoordinates(const views::View* view) {
  gfx::Point center = view->GetLocalBounds().CenterPoint();
  center.set_x(center.x() + view->GetLocalBounds().width() / 4);
  views::View::ConvertPointToScreen(view, &center);
  return center;
}

}  // namespace

class QuitDraggingObserver {
 public:
  explicit QuitDraggingObserver(TabStrip* tab_strip) {
    tab_strip->GetDragContext()->SetDragControllerCallbackForTesting(
        base::BindOnce(&QuitDraggingObserver::OnDragControllerSet,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  QuitDraggingObserver(const QuitDraggingObserver&) = delete;
  QuitDraggingObserver& operator=(const QuitDraggingObserver&) = delete;
  ~QuitDraggingObserver() = default;

  // The observer should be constructed prior to initiating the drag. To prevent
  // misuse via constructing a temporary object, Wait is marked lvalue-only.
  void Wait() & { run_loop_.Run(); }

 private:
  void OnDragControllerSet(TabDragController* controller) {
    controller->SetDragLoopDoneCallbackForTesting(base::BindOnce(
        &QuitDraggingObserver::Quit, weak_ptr_factory_.GetWeakPtr()));
  }

  void Quit() { run_loop_.QuitWhenIdle(); }

  base::RunLoop run_loop_;

  base::WeakPtrFactory<QuitDraggingObserver> weak_ptr_factory_{this};
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
      result += base::NumberToString(user_data->id());
    else
      result += "?";
  }
  return result;
}

TabStrip* GetTabStripForBrowser(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->tabstrip();
}

TabDragController* GetTabDragController(TabStrip* tab_strip) {
  return tab_strip->GetDragContext()->GetDragController();
}

}  // namespace test

using test::GetTabDragController;
using test::GetTabStripForBrowser;
using test::IDString;
using test::ResetIDs;
using test::SetID;
using ui_test_utils::GetCenterInScreenCoordinates;

TabDragControllerTest::TabDragControllerTest()
    : browser_list(BrowserList::GetInstance()) {}

TabDragControllerTest::~TabDragControllerTest() = default;

void TabDragControllerTest::StopAnimating(TabStrip* tab_strip) {
  tab_strip->StopAnimating(true);
}

void TabDragControllerTest::AddTabsAndResetBrowser(Browser* browser,
                                                   int additional_tabs,
                                                   const GURL& url) {
  for (int i = 0; i < additional_tabs; i++) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser, url,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    observer.Wait();
  }
  browser->window()->Show();
  StopAnimating(GetTabStripForBrowser(browser));
  // Perform any scheduled layouts so the tabstrip is in a steady state.
  BrowserView::GetBrowserViewForBrowser(browser)
      ->GetWidget()
      ->LayoutRootViewIfNecessary();
  ResetIDs(browser->tab_strip_model(), 0);
}

Browser* TabDragControllerTest::CreateAnotherBrowserAndResize() {
  Browser* browser2 = CreateBrowser(browser()->profile());
  ResetIDs(browser2->tab_strip_model(), 100);
  Resize(browser(), browser2);
  return browser2;
}

void TabDragControllerTest::Resize(Browser* browser1, Browser* browser2) {
  // Resize the two windows so they're right next to each other.
  const gfx::NativeWindow window = browser1->window()->GetNativeWindow();
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  const gfx::Size size(work_area.width() / 3, work_area.height() / 2);
  gfx::Rect browser_rect(work_area.origin(), size);
  browser1->window()->SetBounds(browser_rect);
  browser_rect.set_x(browser_rect.right());
  browser2->window()->SetBounds(browser_rect);
}

void TabDragControllerTest::SetWindowFinderForTabStrip(
    TabStrip* tab_strip,
    std::unique_ptr<WindowFinder> window_finder) {
  ASSERT_TRUE(GetTabDragController(tab_strip));
  GetTabDragController(tab_strip)->window_finder_ = std::move(window_finder);
}

void TabDragControllerTest::HandleGestureEvent(TabStrip* tab_strip,
                                               ui::GestureEvent* event) {
  // Manually dispatch the event to the drag context (which has capture during
  // drags). Mouse/keyboard/touch events can be dispatched normally via test
  // machinery in ui_test_utils that have no direct gesture event equivalent.
  tab_strip->GetDragContext()->OnGestureEvent(event);
}

bool TabDragControllerTest::HasDragStarted(TabStrip* tab_strip) const {
  return GetTabDragController(tab_strip) &&
         GetTabDragController(tab_strip)->started_drag();
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::WindowState::Get(browser->window()->GetNativeWindow())
      ->is_dragged();
#else
  return false;
#endif
}

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA)

// Following classes verify a crash scenario. Specifically on Windows when focus
// changes it can trigger capture being lost. This was causing a crash in tab
// dragging as it wasn't set up to handle this scenario. These classes
// synthesize this scenario.

// Allows making ClearNativeFocus() invoke ReleaseCapture().
class TestDesktopBrowserFrameAura : public DESKTOP_BROWSER_FRAME_AURA {
 public:
  TestDesktopBrowserFrameAura(BrowserFrame* browser_frame,
                              BrowserView* browser_view)
      : DESKTOP_BROWSER_FRAME_AURA(browser_frame, browser_view) {}
  TestDesktopBrowserFrameAura(const TestDesktopBrowserFrameAura&) = delete;
  TestDesktopBrowserFrameAura& operator=(const TestDesktopBrowserFrameAura&) =
      delete;
  ~TestDesktopBrowserFrameAura() override = default;

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
  bool release_capture_ = false;
};

// Factory for creating a TestDesktopBrowserFrameAura.
class TestNativeBrowserFrameFactory : public NativeBrowserFrameFactory {
 public:
  TestNativeBrowserFrameFactory() = default;
  TestNativeBrowserFrameFactory(const TestNativeBrowserFrameFactory&) = delete;
  TestNativeBrowserFrameFactory& operator=(
      const TestNativeBrowserFrameFactory&) = delete;
  ~TestNativeBrowserFrameFactory() override = default;

  NativeBrowserFrame* Create(BrowserFrame* browser_frame,
                             BrowserView* browser_view) override {
    return new TestDesktopBrowserFrameAura(browser_frame, browser_view);
  }
};

class TabDragCaptureLostTest : public TabDragControllerTest {
 public:
  TabDragCaptureLostTest() {
    NativeBrowserFrameFactory::Set(new TestNativeBrowserFrameFactory);
  }
  TabDragCaptureLostTest(const TabDragCaptureLostTest&) = delete;
  TabDragCaptureLostTest& operator=(const TabDragCaptureLostTest&) = delete;
};

// See description above for details.
IN_PROC_BROWSER_TEST_F(TabDragCaptureLostTest, ReleaseCaptureOnDrag) {
  AddTabsAndResetBrowser(browser(), 1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_1_center) &&
              ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  TestDesktopBrowserFrameAura* frame =
      static_cast<TestDesktopBrowserFrameAura*>(
          BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->
          native_widget_private());
  // Invoke ReleaseCaptureOnDrag() so that when the drag happens and focus
  // changes capture is released and the drag cancels.
  frame->ReleaseCaptureOnNextClear();
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

#endif

IN_PROC_BROWSER_TEST_F(TabDragControllerTest, GestureEndShouldEndDragTest) {
  AddTabsAndResetBrowser(browser(), 1);

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
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

class DetachToBrowserTabDragControllerTest
    : public TabDragControllerTest,
      public ::testing::WithParamInterface<testing::tuple<bool, const char*>> {
 public:
  DetachToBrowserTabDragControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/std::get<0>(GetParam())
            ? std::vector<base::test::FeatureRef>{features::kSplitTabStrip}
            : std::vector<base::test::FeatureRef>{},
        /*disabled_features=*/{
          features::kWebUITabStrip,
#if BUILDFLAG(IS_WIN)
              // Disable NativeWinOcclusion to avoid it interfering with test
              // for dragging over occluded browser window.
              features::kCalculateNativeWinOcclusion,
#endif  // BUILDFLAG(IS_WIN)
        });
  }
  DetachToBrowserTabDragControllerTest(
      const DetachToBrowserTabDragControllerTest&) = delete;
  DetachToBrowserTabDragControllerTest& operator=(
      const DetachToBrowserTabDragControllerTest&) = delete;

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    root_ = browser()->window()->GetNativeWindow()->GetRootWindow();
    // Disable flings which might otherwise inadvertently be generated from
    // tests' touch events.
    ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(
        std::numeric_limits<float>::max());
#endif
#if BUILDFLAG(IS_MAC)
    // Currently MacViews' browser windows are shown in the background and could
    // be obscured by other windows if there are any. This should be fixed in
    // order to be consistent with other platforms.
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#endif  // BUILDFLAG(IS_MAC)
  }

  InputSource input_source() const {
    return strstr(std::get<1>(GetParam()), "mouse") ? INPUT_SOURCE_MOUSE
                                                    : INPUT_SOURCE_TOUCH;
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool SendTouchEventsSync(int action, int id, const gfx::Point& location) {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    if (!ui_controls::SendTouchEventsNotifyWhenDone(
            action, id, location.x(), location.y(), run_loop.QuitClosure())) {
      return false;
    }
    run_loop.Run();
    return true;
  }
#endif

  // The following methods update one of the mouse or touch input depending upon
  // the InputSource.
  bool PressInput(const gfx::Point& location, int id = 0) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseMoveSync(location) &&
          ui_test_utils::SendMouseEventsSync(
              ui_controls::LEFT, ui_controls::DOWN);
    }
#if BUILDFLAG(IS_CHROMEOS)
    return SendTouchEventsSync(ui_controls::kTouchPress, id, location);
#else
    NOTREACHED_NORETURN();
#endif
  }

  bool DragInputTo(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_test_utils::SendMouseMoveSync(location);
#if BUILDFLAG(IS_CHROMEOS)
    return SendTouchEventsSync(ui_controls::kTouchMove, 0, location);
#else
    NOTREACHED_NORETURN();
#endif
  }

  bool DragInputToAsync(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_controls::SendMouseMove(location.x(), location.y());
#if BUILDFLAG(IS_CHROMEOS)
    return ui_controls::SendTouchEvents(ui_controls::kTouchMove, 0,
                                        location.x(), location.y());
#else
    NOTREACHED_NORETURN();
#endif
  }

  bool DragInputToNotifyWhenDone(const gfx::Point& location,
                                 base::OnceClosure task) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_controls::SendMouseMoveNotifyWhenDone(
          location.x(), location.y(), std::move(task));
    }

#if BUILDFLAG(IS_CHROMEOS)
    return ui_controls::SendTouchEventsNotifyWhenDone(
        ui_controls::kTouchMove, 0, location.x(), location.y(),
        std::move(task));
#else
    NOTREACHED_NORETURN();
#endif
  }

  bool ReleaseInput(int id = 0, bool async = false) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return async ? ui_controls::SendMouseEvents(ui_controls::LEFT,
                                                  ui_controls::UP)
                   : ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                        ui_controls::UP);
    }
#if BUILDFLAG(IS_CHROMEOS)
    return async ? ui_controls::SendTouchEvents(ui_controls::kTouchRelease, id,
                                                0, 0)
                 : SendTouchEventsSync(ui_controls::kTouchRelease, id,
                                       gfx::Point());
#else
    NOTREACHED_NORETURN();
#endif
  }

  void ReleaseInputAfterWindowDetached(int first_dragged_tab_width) {
    // On macOS, we want to avoid generating the input event [which requires an
    // associated window] until the window has been detached. Failure to do so
    // causes odd behavior [e.g. on macOS 10.10, the mouse-up will reactivate
    // the first window].
    if (browser_list->size() != 2u) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DetachToBrowserTabDragControllerTest::
                             ReleaseInputAfterWindowDetached,
                         base::Unretained(this), first_dragged_tab_width),
          base::Milliseconds(1));
      return;
    }

    // The tab getting dragged into the new browser should have the same width
    // as before it was dragged.
    EXPECT_EQ(first_dragged_tab_width,
              GetTabStripForBrowser(browser_list->get(1))->tab_at(0)->width());

    // Windows hangs if you use a sync mouse event here.
    ASSERT_TRUE(ReleaseInput(0, true));
  }

  bool MoveInputTo(const gfx::Point& location) {
    aura::Env::GetInstance()->SetLastMouseLocation(location);
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_test_utils::SendMouseMoveSync(location);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return SendTouchEventsSync(ui_controls::kTouchMove, 0, location);
#else
    NOTREACHED_NORETURN();
#endif
  }

  void AddBlankTabAndShow(Browser* browser) {
    InProcessBrowserTest::AddBlankTabAndShow(browser);
  }

  // Returns true if the tab dragging info is correctly set on the attached
  // browser window. On Chrome OS, the info includes two window properties.
  bool IsTabDraggingInfoSet(TabStrip* attached_tabstrip,
                            TabStrip* source_tabstrip) {
    DCHECK(attached_tabstrip);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    aura::Window* dragged_window =
        test::GetWindowForTabStrip(attached_tabstrip);
    attached_tabstrip->GetWidget()->GetNativeWindow();
    return dragged_window->GetProperty(ash::kIsDraggingTabsKey);
#else
    return true;
#endif
  }

  // Returns true if the tab dragging info is correctly cleared on the attached
  // browser window.
  bool IsTabDraggingInfoCleared(TabStrip* attached_tabstrip) {
    DCHECK(attached_tabstrip);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    aura::Window* dragged_window =
        test::GetWindowForTabStrip(attached_tabstrip);
    return !dragged_window->GetProperty(ash::kIsDraggingTabsKey);
#else
    return true;
#endif
  }

  void DragTabAndNotify(TabStrip* tab_strip,
                        base::OnceClosure task,
                        int tab = 0,
                        int drag_x_offset = 0) {
    test::QuitDraggingObserver observer(tab_strip);
    // Move to the tab and drag it enough so that it detaches.
    const gfx::Point tab_0_center =
        GetCenterInScreenCoordinates(tab_strip->tab_at(tab));
    ASSERT_TRUE(PressInput(tab_0_center));
    ASSERT_TRUE(DragInputToNotifyWhenDone(
        tab_0_center + gfx::Vector2d(drag_x_offset, GetDetachY(tab_strip)),
        std::move(task)));
    observer.Wait();
  }

  void DragToDetachGroupAndNotify(TabStrip* tab_strip,
                                  base::OnceClosure task,
                                  tab_groups::TabGroupId group,
                                  int drag_x_offset = 0) {
    test::QuitDraggingObserver observer(tab_strip);
    // Move to the tab and drag it enough so that it detaches.
    const gfx::Point group_header_center =
        GetCenterInScreenCoordinates(tab_strip->group_header(group));
    ASSERT_TRUE(PressInput(group_header_center));
    ASSERT_TRUE(DragInputToNotifyWhenDone(
        group_header_center +
            gfx::Vector2d(drag_x_offset, GetDetachY(tab_strip)),
        std::move(task)));
    observer.Wait();
  }

  // Helper method to click the first tab. Used to ensure no additional widgets
  // are in focus. For example, the tab editor bubble  is automatically opened
  // upon creating a new group.
  void EnsureFocusToTabStrip(TabStrip* tab_strip) {
    ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
    ASSERT_TRUE(ReleaseInput());
  }

  Browser* browser() const { return InProcessBrowserTest::browser(); }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The root window for the event generator.
  raw_ptr<aura::Window, DanglingUntriaged | ExperimentalAsh> root_ = nullptr;
#endif
  base::test::ScopedFeatureList scoped_feature_list_;
  absl::optional<web_app::AppId> tabbed_app_id_;
};

// Creates a browser with four tabs. The first three belong in the same Tab
// Group. Dragging the third tab to after the fourth tab will result in a
// removal of the dragged tab from its group. Then dragging the second tab to
// after the third tab will also result in a removal of that dragged tab from
// its group.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragRightToUngroupTab) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({0, 1, 2});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(3u, model->group_model()->GetTabGroup(group)->ListTabs().length());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the second index to the tab in the third index switches
  // the tabs and removes the dragged tab from the group.
  EXPECT_EQ("0 1 3 2", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(0, 2));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(2));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(3));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the first index to the tab in the second index (tab 3)
  // switches the tabs and removes the dragged tab from the group.
  EXPECT_EQ("0 3 1 2", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(0, 1));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(1));
}

// Creates a browser with four tabs. The last three belong in the same Tab
// Group. Dragging the second tab to before the first tab will result in a
// removal of the dragged tab from its group. Then dragging the third tab to
// before the second tab will also result in a removal of that dragged tab from
// its group.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragLeftToUngroupTab) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({1, 2, 3});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(3u, model->group_model()->GetTabGroup(group)->ListTabs().length());
  EXPECT_EQ(group, model->GetTabGroupForTab(1).value());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the first index to the tab in the zero-th index
  // switches the tabs and removes the dragged tab from the group.
  EXPECT_EQ("1 0 2 3", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(2, 4));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(0));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(1));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the second index to the tab in the first index switches
  // the tabs and removes the dragged tab from the group.
  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(3, 4));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(2));
}

// Creates a browser with four tabs. The first three belong in the same Tab
// Group. Dragging tabs in a tab group within the defined threshold does not
// modify the group of the dragged tab.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragTabWithinGroupDoesNotModifyGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({0, 1, 2});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(0, 3));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the zero-th index to the tab in the first index
  // switches the tabs but group membership does not change.
  EXPECT_EQ("1 0 2 3", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(0, 3));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Dragging the tab in the second index to the tab in the first index switches
  // the tabs but group membership does not change.
  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(0, 3));
}

// Creates a browser with four tabs. The first tab is in a Tab Group. Dragging
// the only tab in that group will remove the group.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragOnlyTabInGroupRemovesGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({0});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(0, 1));

  // Dragging the tab in the zero-th index to the tab in the first index
  // switches the tabs and removes the group of the zero-th tab.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 0 2 3", IDString(model));
  EXPECT_FALSE(model->group_model()->ContainsTabGroup(group));
}

// Creates a browser with four tabs. The first tab is in Tab Group 1. The
// third tab is in Tab Group 2. Dragging the second tab over one to the left
// will result in the second tab joining Tab Group 1. Then dragging the third
// tab over one to the left will result in the tab joining Tab Group 1. Then
// dragging the fourth tab over one to the left will result in the tab joining
// Tab Group 1 as well.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragSingleTabLeftIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({0});
  model->AddToNewGroup({2});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  // Dragging the tab in the first index toward the tab in the zero-th index
  // switches the tabs and adds the dragged tab to the group.
  // Drag only half the tab length, to ensure that the tab in the first index
  // lands in the slot between the tab in the zero-th index and the group
  // header to the left of the zero-th index.
  const gfx::Point drag_origin =
      GetCenterInScreenCoordinates(tab_strip->tab_at(1));
  const gfx::Vector2d drag_amount =
      gfx::Vector2d(-tab_strip->tab_at(0)->width() / 2, 0);
  ASSERT_TRUE(PressInput(drag_origin));
  ASSERT_TRUE(DragInputTo(drag_origin + drag_amount));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 0 2 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 2));

  // Dragging the tab in the second index to the tab in the first index switches
  // the tabs and adds the dragged tab to the group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 3));

  // Dragging the tab in the third index to the tab in the second index switches
  // the tabs and adds the dragged tab to the group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 2 3 0", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 4));
}

// Creates a browser with five tabs. The fourth tab is in Tab Group 1. The
// second tab is in Tab Group 2. Dragging the third tab over one to the right
// will result in the tab joining Tab Group 1. Then dragging the second tab
// over one to the right will result in the tab joining Tab Group 1. Then
// dragging the first tab over one to the right will result in the tab joining
// Tab Group 1 as well.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragSingleTabRightIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 4);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({3});
  model->AddToNewGroup({1});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  // Dragging the tab in the second index to the tab in the third index switches
  // the tabs and adds the dragged tab to the group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("0 1 3 2 4", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(2, 4));

  // Dragging the tab in the first index to the tab in the second index switches
  // the tabs and adds the dragged tab to the group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("0 3 1 2 4", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(1, 4));

  // Dragging the tab in the zero-th index to the tab in the first index
  // switches the tabs and adds the dragged tab to the group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("3 0 1 2 4", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 4));
}

// Creates a browser with five tabs. The last two tabs are in a Tab Group.
// Dragging the first tab past the last slot should allow it to exit the group.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragSingleTabRightOfRightmostGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 4);
  tab_groups::TabGroupId group = model->AddToNewGroup({3, 4});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  // Dragging the first tab past the last slot brings it to the end of the
  // tabstrip, where it should not be in a group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(4)) +
                          gfx::Vector2d(1, 0)));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 2 3 4 0", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group)->ListTabs(), gfx::Range(2, 4));
}

// Creates a browser with four tabs each in its own group. Selecting and
// dragging the first and third tabs right at the first tab will result in the
// tabs joining the same group as the tab in the second position. Then dragging
// the tabs over two to the right will result in the tabs joining the same group
// as the last tab.
// Flaky on windows. http://crbug.com/1164561
#if BUILDFLAG(IS_WIN)
#define MAYBE_DragMultipleTabsRightIntoGroup \
  DISABLED_DragMultipleTabsRightIntoGroup
#else
#define MAYBE_DragMultipleTabsRightIntoGroup DragMultipleTabsRightIntoGroup
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragMultipleTabsRightIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  model->AddToNewGroup({0});
  tab_groups::TabGroupId group2 = model->AddToNewGroup({1});
  model->AddToNewGroup({2});
  tab_groups::TabGroupId group4 = model->AddToNewGroup({3});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  // Click the first tab and select third tab so both first and third tabs are
  // selected.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  browser()->tab_strip_model()->ToggleSelectionAt(2);

  // Dragging the first tab at the leftmost position to the center of the first
  // tab will result in the tabs joining the beginning of Tab Group 2.
  const gfx::Point center_first_tab =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  ASSERT_TRUE(
      PressInput(test::GetLeftCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(center_first_tab));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("0 2 1 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group4)->ListTabs(), gfx::Range(3, 4));
  EXPECT_EQ(group_model->GetTabGroup(group2)->ListTabs(), gfx::Range(0, 3));

  // Dragging the center of the first tab to the center of the third tab will
  // result in the tabs joining the end of Tab Group 4.
  const gfx::Point center_third_tab =
      GetCenterInScreenCoordinates(tab_strip->tab_at(2));
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(center_third_tab));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 3 0 2", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group2)->ListTabs(), gfx::Range(0, 1));
  EXPECT_EQ(group_model->GetTabGroup(group4)->ListTabs(), gfx::Range(1, 4));
}

// Creates a browser with four tabs each in its own group. Selecting and
// dragging the second and fourth tabs left at the fourth tab will result in the
// tabs joining the same group as the tab in the third position.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragMultipleTabsLeftIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({0});
  model->AddToNewGroup({1});
  tab_groups::TabGroupId group3 = model->AddToNewGroup({2});
  model->AddToNewGroup({3});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  // Click the second tab and select fourth tab so both second and fourth tabs
  // are selected.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  browser()->tab_strip_model()->ToggleSelectionAt(3);

  // Dragging the fourth tab slightly to the left will result in the two
  // selected tabs joining the end of Tab Group 3.
  const gfx::Point left_center_fourth_tab =
      test::GetLeftCenterInScreenCoordinates(tab_strip->tab_at(3));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(DragInputTo(left_center_fourth_tab));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("0 2 1 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 1));
  EXPECT_EQ(group_model->GetTabGroup(group3)->ListTabs(), gfx::Range(1, 4));
}

// Creates a browser with four tabs with third and fourth in a group.
// Selecting and  dragging the second and third tabs towards left out
// of the group will result in the tabs being ungrouped.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragUngroupedTabGroupedTabOutsideGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({2, 3});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  // Click the second tab and select third tab so both second and third tabs
  // are selected.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  browser()->tab_strip_model()->ToggleSelectionAt(2);

  // Dragging the third tab slightly to the left will result in the two
  // selected tabs leaving the group.
  const gfx::Point first_tab_screen_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(first_tab_screen_center));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(3, 4));
}

// Creates a browser with four tabs. The first two tabs are in Tab Group 1.
// Dragging the third tab over one to the left will result in the tab joining
// Tab Group 1. While this drag is still in session, pressing escape will revert
// group of the tab to before the drag session started.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       RevertDragSingleTabIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({0, 1});
  StopAnimating(tab_strip);

  // Dragging the tab in the second index to the tab in the first index switches
  // the tabs and adds the dragged tab to the group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));

  EXPECT_EQ("0 2 1 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 3));

  ASSERT_TRUE(TabDragController::IsActive());

  // Pressing escape will revert the tabs to original state before the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  EXPECT_EQ("0 1 2 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 2));
}

// Creates a browser with four tabs. The last two tabs are in Tab Group 1. The
// second tab is in Tab Group 2. Dragging the second tab over one to the right
// will result in the tab joining Tab Group 1. While this drag is still in
// session, pressing escape will revert group of the tab to before the drag
// session started.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       RevertDragSingleTabGroupIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({2, 3});
  tab_groups::TabGroupId group2 = model->AddToNewGroup({1});
  const tab_groups::TabGroupVisualData new_data(
      u"Foo", tab_groups::TabGroupColorId::kCyan);
  group_model->GetTabGroup(group2)->SetVisualData(new_data);
  StopAnimating(tab_strip);

  // Dragging the tab in the first index to the tab in the second index switches
  // the tabs and adds the dragged tab to the group.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));

  EXPECT_EQ("0 2 1 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(1, 4));

  ASSERT_TRUE(TabDragController::IsActive());

  // Pressing escape will revert the tabs to original state before the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  EXPECT_EQ("0 1 2 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(2, 4));
  EXPECT_EQ(group_model->GetTabGroup(group2)->ListTabs(), gfx::Range(1, 2));
  const tab_groups::TabGroupVisualData* group2_visual_data =
      group_model->GetTabGroup(group2)->visual_data();
  EXPECT_THAT(group2_visual_data->title(), new_data.title());
  EXPECT_THAT(group2_visual_data->color(), new_data.color());
}

// Creates a browser with four tabs. The middle two belong in the same Tab
// Group. Dragging the group header will result in both the grouped tabs moving
// together.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragGroupHeaderDragsGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({1, 2});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  ASSERT_EQ(4, model->count());
  ASSERT_EQ(2u, group_model->GetTabGroup(group)->ListTabs().length());

  // Drag the entire group right by its header.
  ASSERT_TRUE(
      PressInput(GetCenterInScreenCoordinates(tab_strip->group_header(group))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("0 3 1 2", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group)->ListTabs(), gfx::Range(2, 4));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(0));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(1));

  // Drag the entire group left by its header.
  ASSERT_TRUE(
      PressInput(GetCenterInScreenCoordinates(tab_strip->group_header(group))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group)->ListTabs(), gfx::Range(0, 2));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(2));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(3));
}

// Creates a browser with four tabs. The first two belong in Tab Group 1, and
// the last two belong in Tab Group 2. Dragging the group header of Tab Group 1
// right will result in Tab Group 1 moving but avoiding Tab Group 2.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragGroupHeaderRightAvoidsOtherGroups) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({0, 1});
  tab_groups::TabGroupId group2 = model->AddToNewGroup({2, 3});
  StopAnimating(tab_strip);

  ASSERT_EQ(4, model->count());
  ASSERT_EQ(2u, group_model->GetTabGroup(group1)->ListTabs().length());
  ASSERT_EQ(2u, group_model->GetTabGroup(group2)->ListTabs().length());

  // Drag group1 right, but not far enough to get to the other side of group2.
  ASSERT_TRUE(PressInput(
      GetCenterInScreenCoordinates(tab_strip->group_header(group1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Expect group1 to "snap back" to its current position, avoiding group2.
  EXPECT_EQ("0 1 2 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 2));
  EXPECT_EQ(group_model->GetTabGroup(group2)->ListTabs(), gfx::Range(2, 4));

  // Drag group1 right, far enough to get to the other side of group2.
  ASSERT_TRUE(PressInput(
      GetCenterInScreenCoordinates(tab_strip->group_header(group1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Expect group1 to "snap to" the other side of group2 and not land in the
  // middle.
  EXPECT_EQ("2 3 0 1", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(2, 4));
  EXPECT_EQ(group_model->GetTabGroup(group2)->ListTabs(), gfx::Range(0, 2));
}

// Creates a browser with four tabs. The first two belong in Tab Group 1, and
// the last two belong in Tab Group 2. Dragging the group header of Tab Group 2
// left will result in Tab Group 2 moving but avoiding Tab Group 1.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragGroupHeaderLeftAvoidsOtherGroups) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group1 = model->AddToNewGroup({0, 1});
  tab_groups::TabGroupId group2 = model->AddToNewGroup({2, 3});
  StopAnimating(tab_strip);

  ASSERT_EQ(4, model->count());
  ASSERT_EQ(2u, group_model->GetTabGroup(group1)->ListTabs().length());
  ASSERT_EQ(2u, group_model->GetTabGroup(group2)->ListTabs().length());

  // Drag group2 left, but not far enough to get to the other side of group1.
  ASSERT_TRUE(PressInput(
      GetCenterInScreenCoordinates(tab_strip->group_header(group2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Expect group2 to "snap back" to its current position, avoiding group1.
  EXPECT_EQ("0 1 2 3", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(0, 2));
  EXPECT_EQ(group_model->GetTabGroup(group2)->ListTabs(), gfx::Range(2, 4));

  // Drag group2 left, far enough to get to the other side of group1.
  ASSERT_TRUE(PressInput(
      GetCenterInScreenCoordinates(tab_strip->group_header(group2))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // Expect group2 to "snap to" the other side of group1 and not land in the
  // middle.
  EXPECT_EQ("2 3 0 1", IDString(model));
  EXPECT_EQ(group_model->GetTabGroup(group1)->ListTabs(), gfx::Range(2, 4));
  EXPECT_EQ(group_model->GetTabGroup(group2)->ListTabs(), gfx::Range(0, 2));
}

// Creates a browser with four tabs. The first tab is pinned, and the last
// three belong in the same Tab Group. Dragging the pinned tab to the middle
// of the group will not result in the pinned tab getting grouped or moving
// into the group.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragPinnedTabDoesNotGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  model->SetTabPinned(0, true);
  tab_groups::TabGroupId group = model->AddToNewGroup({1, 2, 3});
  StopAnimating(tab_strip);

  ASSERT_EQ(4, model->count());
  ASSERT_EQ(3u, model->group_model()->GetTabGroup(group)->ListTabs().length());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  // The pinned tab should not have moved or joined the group.
  EXPECT_EQ("0 1 2 3", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(1, 4));
  EXPECT_EQ(absl::nullopt, model->GetTabGroupForTab(0));
}

// Drags a tab within the window (without dragging the whole window) then
// pressing a key ends the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       KeyPressShouldEndDragTest) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));

  ASSERT_TRUE(TabDragController::IsActive());

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  StopAnimating(tab_strip);

  EXPECT_EQ("1 0", IDString(browser()->tab_strip_model()));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

#if defined(USE_AURA)
bool SubtreeShouldBeExplored(aura::Window* window,
                             const gfx::Point& local_point) {
  gfx::Point point_in_parent = local_point;
  aura::Window::ConvertPointToTarget(window, window->parent(),
                                     &point_in_parent);
  gfx::Point point_in_root = local_point;
  aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                     &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, point_in_parent, point_in_root,
                       base::TimeTicks::Now(), 0, 0);
  return window->targeter()->SubtreeShouldBeExploredForEvent(window, event);
}

// The logic to find the target tabstrip should take the window mask into
// account. This test hangs without the fix. crbug.com/473080.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragWithMaskedWindows) {
  AddTabsAndResetBrowser(browser(), 1);

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  const gfx::Rect bounds = browser_window->GetBoundsInScreen();
  aura::test::TestWindowDelegate masked_window_delegate;
  masked_window_delegate.set_can_focus(false);
  std::unique_ptr<aura::Window> masked_window(
      aura::test::CreateTestWindowWithDelegate(
          &masked_window_delegate, 10, bounds, browser_window->parent()));
  masked_window->SetProperty(aura::client::kZOrderingKey,
                             ui::ZOrderLevel::kFloatingWindow);
  auto targeter = std::make_unique<aura::WindowTargeter>();
  targeter->SetInsets(gfx::Insets::TLBR(0, bounds.width() - 10, 0, 0));
  masked_window->SetEventTargeter(std::move(targeter));

  ASSERT_FALSE(SubtreeShouldBeExplored(masked_window.get(),
                                       gfx::Point(bounds.width() - 11, 0)));
  ASSERT_TRUE(SubtreeShouldBeExplored(masked_window.get(),
                                      gfx::Point(bounds.width() - 9, 0)));
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}
#endif  // USE_AURA

namespace {

// Invoked from the nested run loop.
void DragToSeparateWindowStep2(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* not_attached_tab_strip,
                               TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Test that after the tabs are detached from the source tabstrip (in this
  // case |not_attached_tab_strip|), the tab dragging info should be properly
  // cleared on the source tabstrip.
  EXPECT_TRUE(test->IsTabDraggingInfoCleared(not_attached_tab_strip));
  // At this moment there should be a new browser window for the dragged tabs.
  EXPECT_EQ(3u, test->browser_list->size());
  Browser* new_browser = test->browser_list->get(2);
  TabStrip* new_tab_strip = GetTabStripForBrowser(new_browser);
  ASSERT_TRUE(new_tab_strip->GetDragContext()->IsDragSessionActive());
  // Test that the tab dragging info should be correctly set on the new window.
  EXPECT_TRUE(
      test->IsTabDraggingInfoSet(new_tab_strip, not_attached_tab_strip));

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  //
  // Note: It's possible on small screens for the windows to overlap, so we want
  // to pick a point squarely within the second tab strip and not in the
  // starting window.
  const gfx::Rect old_window_bounds =
      not_attached_tab_strip->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect target_bounds = target_tab_strip->GetBoundsInScreen();
  gfx::Point target_point = target_bounds.CenterPoint();
  target_point.set_x(std::max(target_point.x(), old_window_bounds.right() + 1));
  ASSERT_TRUE(target_bounds.Contains(target_point));
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Flaky. https://crbug.com/1176998
#if (BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)) || BUILDFLAG(IS_LINUX)
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
#define MAYBE_DragToSeparateWindow DISABLED_DragToSeparateWindow
#else
#define MAYBE_DragToSeparateWindow DragToSeparateWindow
#endif

// Creates two browsers, drags from first into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragToSeparateWindowStep2, this,
                                             tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip));
  EXPECT_TRUE(IsTabDraggingInfoSet(tab_strip2, tab_strip));

  // Drag to the trailing end of the tabstrip to ensure we're in a predictable
  // spot within the strip.
  StopAnimating(tab_strip2);
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip2->tab_at(1))));

  // Release mouse or touch, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
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

#if BUILDFLAG(IS_WIN)

// Create two browsers, with the second one occluded, and drag from first over
// second. This should create a third browser, w/o bringing forward the second
// browser, because it's occluded.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToOccludedWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Mark the second browser as occluded. NativeWindow occlusion calculation has
  // been disabled in test constructor, so we don't need an actual occluding
  // window.
  browser2->window()
      ->GetNativeWindow()
      ->GetHost()
      ->SetNativeWindowOcclusionState(aura::Window::OcclusionState::OCCLUDED,
                                      {});

  // Drag a tab from first browser to middle of first tab of the second,
  // occluded browser, and drop. This should create a third browser window.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(
      DragInputToAsync(GetCenterInScreenCoordinates(tab_strip2->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());

  EXPECT_EQ(3u, browser_list->size());
}

#endif  // BUILDFLAG(IS_WIN)

namespace {

// WindowFinder that calls OnMouseCaptureLost() from
// GetLocalProcessWindowAtPoint().
class CaptureLoseWindowFinder : public WindowFinder {
 public:
  explicit CaptureLoseWindowFinder(TabStrip* tab_strip)
      : tab_strip_(tab_strip) {}
  CaptureLoseWindowFinder(const CaptureLoseWindowFinder&) = delete;
  CaptureLoseWindowFinder& operator=(const CaptureLoseWindowFinder&) = delete;
  ~CaptureLoseWindowFinder() override = default;

  // WindowFinder:
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override {
    static_cast<views::View*>(tab_strip_->GetDragContext())
        ->OnMouseCaptureLost();
    return nullptr;
  }

 private:
  raw_ptr<TabStrip> tab_strip_;
};

}  // namespace

// Calls OnMouseCaptureLost() from WindowFinder::GetLocalProcessWindowAtPoint()
// and verifies we don't crash. This simulates a crash seen on windows.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       CaptureLostDuringDrag) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Press on first tab so drag is active. Reset WindowFinder to one that causes
  // capture to be lost from within GetLocalProcessWindowAtPoint(), then
  // continue drag. The capture lost should trigger the drag to cancel.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  SetWindowFinderForTabStrip(
      tab_strip, base::WrapUnique(new CaptureLoseWindowFinder(tab_strip)));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsWindowPositionManaged(aura::Window* window) {
  return window->GetProperty(ash::kWindowPositionManagedTypeKey);
}
bool HasUserChangedWindowPositionOrSize(aura::Window* window) {
  return ash::WindowState::Get(window)->bounds_changed_by_user();
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
  MaximizedBrowserWindowWaiter(const MaximizedBrowserWindowWaiter&) = delete;
  MaximizedBrowserWindowWaiter& operator=(const MaximizedBrowserWindowWaiter&) =
      delete;
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
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(&MaximizedBrowserWindowWaiter::CheckMaximized),
              base::Unretained(this)));
      return false;
    }

    // Quit the run_loop to end the wait.
    if (!quit_.is_null())
      std::move(quit_).Run();
    return true;
  }

  // The browser window observed by this waiter.
  raw_ptr<BrowserWindow> window_;

  // The waiter's RunLoop quit closure.
  base::RepeatingClosure quit_;
};

}  // namespace

// Drags from browser to separate window and releases mouse.
// TODO(crbug/1444028): The test is failing. Please re-enable after fixing the
// failure.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_DetachToOwnWindow DISABLED_DetachToOwnWindow
#else
#define MAYBE_DetachToOwnWindow DetachToOwnWindow
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DetachToOwnWindow) {
  const gfx::Rect initial_bounds(browser()->window()->GetBounds());
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  int tab_0_width = tab_strip->tab_at(0)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_0_width));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);

  EXPECT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  EXPECT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindowWithNonVisibleOnAllWorkspaceState) {
  // Set the source browser to be visible on all workspace.
  ASSERT_EQ(1u, browser_list->size());
  Browser* source_browser = browser_list->get(0);
  auto* source_window = source_browser->window()->GetNativeWindow();
  source_window->SetProperty(
      aura::client::kWindowWorkspaceKey,
      aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);

  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  int tab_0_width = tab_strip->tab_at(0)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_0_width));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);

  EXPECT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  EXPECT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));
  EXPECT_FALSE(GetIsDragged(new_browser));
  // After this both windows should still be manageable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(
      IsWindowPositionManaged(new_browser->window()->GetNativeWindow()));

  auto* new_window = new_browser->window()->GetNativeWindow();
  // The new window should not be visible on all workspace.
  ASSERT_FALSE(new_window->GetProperty(aura::client::kWindowWorkspaceKey) ==
               aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
}
#endif  // BUILDFLAG(IS_CHROME_ASH)

// Tests that a tab can be dragged from a browser window that is resized to full
// screen.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachFromFullsizeWindow) {
  // Resize the browser window so that it is as big as the work area.
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .work_area();
  browser()->window()->SetBounds(work_area);
  const gfx::Rect initial_bounds(browser()->window()->GetBounds());
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  int tab_0_width = tab_strip->tab_at(0)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_0_width));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

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

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
  EXPECT_FALSE(tab_strip2->GetWidget()->HasCapture());
}

// This test doesn't make sense on Mac, since it has no concept of "maximized".
#if !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/1476904) Test is flaky on ChromeOS/Lacros.
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

  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  int tab_0_width = tab_strip->tab_at(0)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_0_width));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

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

  const bool kMaximizedStateRetainedOnTabDrag =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
      false;
#else
      true;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

  if (kMaximizedStateRetainedOnTabDrag) {
    // The new window should be maximized.
    MaximizedBrowserWindowWaiter(new_browser->window()).Wait();
  }
  EXPECT_EQ(new_browser->window()->IsMaximized(),
            kMaximizedStateRetainedOnTabDrag);
}
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)

// This test makes sense only on Chrome OS where we have the immersive
// fullscreen mode. The detached tab to a new browser window should remain in
// immersive fullscreen mode.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindowWhileInImmersiveFullscreenMode) {
  // Toggle the immersive fullscreen mode for the initial browser.
  chrome::ToggleFullscreenMode(browser());
  ImmersiveModeController* controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->immersive_mode_controller();
  ASSERT_TRUE(controller->IsEnabled());

  // Forcively reveal the tabstrip immediately.
  std::unique_ptr<ImmersiveRevealedLock> lock =
      controller->GetRevealedLock(ImmersiveModeController::ANIMATE_REVEAL_NO);

  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  int tab_0_width = tab_strip->tab_at(0)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_0_width));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

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
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
  EXPECT_FALSE(GetIsDragged(browser()));
}

// Replaces a tab being dragged before the user moved enough to start a drag.
#if (BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH))
// TODO(crbug.com/1466682) Test is flaky on ChromeOS/Lacros - releasing input
// does not always end the drag.
#define MAYBE_ReplaceBeforeStartedDragging DISABLED_ReplaceBeforeStartedDragging
#else
#define MAYBE_ReplaceBeforeStartedDragging ReplaceBeforeStartedDragging
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_ReplaceBeforeStartedDragging) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  // A drag session should exist, but the drag should not have started.
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_FALSE(HasDragStarted(tab_strip));

  // Replace the tab being dragged.
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  browser()->tab_strip_model()->ReplaceWebContentsAt(
      0, std::move(new_web_contents));

  // The drag session should still exist, and still not be started.
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_FALSE(HasDragStarted(tab_strip));

  // Move the mouse enough to start the drag.  It doesn't matter whether this
  // is enough to create a window or not.
  ASSERT_TRUE(DragInputTo(gfx::Point(tab_0_center.x() + 40, tab_0_center.y())));

  // Drag should now have started.
  ASSERT_TRUE(HasDragStarted(tab_strip));

  // The replaced webcontents should not have an id character.
  EXPECT_EQ("? 1", IDString(browser()->tab_strip_model()));

  EXPECT_TRUE(ReleaseInput());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragDoesntStartFromClick) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  EXPECT_TRUE(PressInput(tab_0_center));

  // A drag session should exist, but the drag should not have started.
  EXPECT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  EXPECT_TRUE(TabDragController::IsActive());
  EXPECT_FALSE(HasDragStarted(tab_strip));

  // Move the mouse enough to start the drag.  It doesn't matter whether this
  // is enough to create a window or not.
  EXPECT_TRUE(DragInputTo(gfx::Point(tab_0_center.x() + 20, tab_0_center.y())));

  // Drag should now have started.
  EXPECT_TRUE(HasDragStarted(tab_strip));

  EXPECT_TRUE(ReleaseInput());
}

// Deletes a tab being dragged while still attached.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteTabWhileAttached) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab and move it enough so that it starts dragging but is
  // still attached.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputTo(
                  gfx::Point(tab_0_center.x() + 20, tab_0_center.y())));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser()));
}

// Selects 1 tab out of 4, drags it out and closes the new browser window while
// dragging.
#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
// TODO(crbug.com/1031801) Test is flaky on Windows and Mac.
#define MAYBE_DeleteTabsWhileDetached DISABLED_DeleteTabsWhileDetached
#else
#define MAYBE_DeleteTabsWhileDetached DeleteTabsWhileDetached
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DeleteTabsWhileDetached) {
  AddTabsAndResetBrowser(browser(), 3);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  EXPECT_EQ("0 1 2 3", IDString(browser()->tab_strip_model()));

  // Drag the third tab out of its browser window, request to close the detached
  // tab and verify its owning window gets properly closed.
  DragTabAndNotify(
      tab_strip, base::BindLambdaForTesting([&]() {
        ASSERT_EQ(2u, browser_list->size());
        Browser* old_browser = browser_list->get(0);
        EXPECT_EQ("0 1 3", IDString(old_browser->tab_strip_model()));
        Browser* new_browser = browser_list->get(1);
        EXPECT_EQ("2", IDString(new_browser->tab_strip_model()));
        chrome::CloseTab(new_browser);
        // Ensure that the newly created tab strip is "closeable" just after
        // requesting to close it, even if we are still waiting for the nested
        // move loop to exit. Regression test for https://crbug.com/1309461.
        EXPECT_TRUE(GetTabStripForBrowser(new_browser)->IsTabStripCloseable());
      }),
      2);

  // Should no longer be dragging.
  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // Dragged out tab (and its owning window) should get closed.
  EXPECT_EQ("0 1 3", IDString(browser()->tab_strip_model()));

  // No longer dragging.
  EXPECT_FALSE(GetIsDragged(browser()));
}

namespace {

void PressEscapeWhileDetachedStep2(const BrowserList* browser_list) {
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      new_browser->window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
      false, false));
}

}  // namespace

// Detaches a tab and while detached presses escape to revert the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       RevertDragWhileDetached) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(
      tab_strip, base::BindOnce(&PressEscapeWhileDetachedStep2, browser_list));

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
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

// Creates a browser with two tabs, drags the second to the first.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest, DragInSameWindow) {
  AddTabsAndResetBrowser(browser(), 1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  // Test that the dragging info is correctly set on |tab_strip|.
  EXPECT_TRUE(IsTabDraggingInfoSet(tab_strip, tab_strip));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  // Test that the dragging info is properly cleared after dragging.
  EXPECT_TRUE(IsTabDraggingInfoCleared(tab_strip));

  // The tab strip should no longer have capture because the drag was ended and
  // mouse/touch was released.
  EXPECT_FALSE(tab_strip->GetWidget()->HasCapture());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       TabDragContextOwnsDraggedTabs) {
  AddTabsAndResetBrowser(browser(), 1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabDragContext* drag_context = tab_strip->GetDragContext();
  Tab* dragged_tab = tab_strip->tab_at(1);

  // Tabs are not in the TabDragContext when they aren't being dragged.
  ASSERT_FALSE(drag_context->Contains(dragged_tab));

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  EXPECT_FALSE(drag_context->Contains(dragged_tab));

  // TabDragContext gets them only after the drag truly begins.
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  EXPECT_TRUE(drag_context->Contains(dragged_tab));

  // TabDragContext keeps them after the drag ends...
  ASSERT_TRUE(ReleaseInput());
  EXPECT_TRUE(drag_context->Contains(dragged_tab));

  // Until they animate back into place.
  StopAnimating(tab_strip);
  EXPECT_FALSE(drag_context->Contains(dragged_tab));

  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_TabDragContextOwnsClosingDraggedTabs \
  DISABLED_TabDragContextOwnsClosingDraggedTabs
#else
#define MAYBE_TabDragContextOwnsClosingDraggedTabs \
  TabDragContextOwnsClosingDraggedTabs
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_TabDragContextOwnsClosingDraggedTabs) {
  AddTabsAndResetBrowser(browser(), 1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabDragContext* drag_context = tab_strip->GetDragContext();
  Tab* dragged_tab = tab_strip->tab_at(1);

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));

  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));

  // Delete the tab being dragged.
  browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // TabDragContext still owns the tab while it's closing.
  ASSERT_TRUE(dragged_tab->closing());
  EXPECT_TRUE(drag_context->Contains(dragged_tab));

  // Completing the close animation destroys the tab.
  views::ViewTracker dragged_tab_tracker(dragged_tab);
  StopAnimating(tab_strip);
  EXPECT_EQ(dragged_tab_tracker.view(), nullptr);
}

namespace {

void DragAllStep2(DetachToBrowserTabDragControllerTest* test,
                  const BrowserList* browser_list) {
  // Should only be one window.
  ASSERT_EQ(1u, browser_list->size());
  // Windows hangs if you use a sync mouse event here.
  ASSERT_TRUE(test->ReleaseInput(0, true));
}

}  // namespace

// Selects multiple tabs and starts dragging the window.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest, DragAll) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  browser()->tab_strip_model()->ToggleSelectionAt(0);
  const gfx::Rect initial_bounds = browser()->window()->GetBounds();

  // Move to the first tab and drag it enough so that it would normally
  // detach.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragAllStep2, this, browser_list));

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
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
                                  TabStrip* target_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  ASSERT_TRUE(
      test->DragInputToAsync(GetCenterInScreenCoordinates(target_tab_strip)));
}

}  // namespace

// Flaky. http://crbug.com/1128774 and http://crbug.com/1176998
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
// These were flaking on all macs, so commented out ARCH_ above for
// crbug.com/1160917 too.
#define MAYBE_DragAllToSeparateWindow DISABLED_DragAllToSeparateWindow
#else
#define MAYBE_DragAllToSeparateWindow DragAllToSeparateWindow
#endif

// Creates two browsers, selects all tabs in first and drags into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragAllToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->ToggleSelectionAt(0);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragAllToSeparateWindowStep2,
                                             this, tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
#define MAYBE_DragWindowIntoGroup DISABLED_DragWindowIntoGroup
#else
#define MAYBE_DragWindowIntoGroup DragWindowIntoGroup
#endif

// Creates two browser with two tabs each. The first browser has one tab in a
// group and the second tab not in a group. The second browser {browser2} has
// two tabs in another group {group1}. Dragging the two tabs in the first
// browser into the middle of the second browser will insert the two dragged
// tabs into the {group1}} after the first tab.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragWindowIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 1);
  model->AddToNewGroup({0});
  StopAnimating(tab_strip);

  // Set up the second browser with two tabs in a group with distinct IDs.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  TabStripModel* model2 = browser2->tab_strip_model();
  AddTabsAndResetBrowser(browser2, 1);
  ResetIDs(model2, 100);
  tab_groups::TabGroupId group1 = model2->AddToNewGroup({0, 1});
  StopAnimating(tab_strip2);

  // Click the first tab and select the second tab so they are the only ones
  // selected.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  browser()->tab_strip_model()->ToggleSelectionAt(1);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragAllToSeparateWindowStep2,
                                             this, tab_strip, tab_strip2));

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());

  EXPECT_EQ("100 0 1 101", IDString(model2));
  EXPECT_EQ(model2->group_model()->GetTabGroup(group1)->ListTabs(),
            gfx::Range(0, 4));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
// Flaky on Mac10.14 and Linux: https://crbug.com/1213345
#define MAYBE_DragGroupHeaderToSeparateWindow \
  DISABLED_DragGroupHeaderToSeparateWindow
#else
#define MAYBE_DragGroupHeaderToSeparateWindow DragGroupHeaderToSeparateWindow
#endif

// Creates two browsers, then drags a group from one to the other.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragGroupHeaderToSeparateWindow) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  AddTabsAndResetBrowser(browser(), 1);
  tab_groups::TabGroupId group = model->AddToNewGroup({0, 1});
  tab_groups::TabGroupColorId group_color = tab_strip->GetGroupColorId(group);
  StopAnimating(tab_strip);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  TabStripModel* model2 = browser2->tab_strip_model();
  StopAnimating(tab_strip2);

  // Drag the group by its header into the second browser.
  DragToDetachGroupAndNotify(tab_strip,
                             base::BindOnce(&DragAllToSeparateWindowStep2, this,
                                            tab_strip, tab_strip2),
                             group);
  ASSERT_TRUE(ReleaseInput());

  // Expect the group to be in browser2, but with a new tab_groups::TabGroupId.
  EXPECT_EQ("100 0 1", IDString(model2));
  std::vector<tab_groups::TabGroupId> groups2 =
      model2->group_model()->ListTabGroups();
  EXPECT_EQ(1u, groups2.size());
  EXPECT_EQ(model2->group_model()->GetTabGroup(groups2[0])->ListTabs(),
            gfx::Range(1, 3));
  EXPECT_EQ(groups2[0], group);
  EXPECT_EQ(tab_strip2->GetGroupColorId(groups2[0]), group_color);
}

// Drags a tab group by the header to a new position toward the right and
// presses escape to revert the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       RevertHeaderDragRight) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({0, 1});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  TabGroupHeader* group_header = tab_strip->group_header(group);
  EXPECT_FALSE(group_header->dragging());
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(group_header)));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(2))));
  ASSERT_TRUE(group_header->dragging());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      browser()->window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
      false, false));
  StopAnimating(tab_strip);

  EXPECT_EQ(1u, browser_list->size());
  EXPECT_FALSE(group_header->dragging());
  EXPECT_EQ("0 1 2 3", IDString(browser()->tab_strip_model()));
  std::vector<tab_groups::TabGroupId> groups =
      model->group_model()->ListTabGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(model->group_model()->GetTabGroup(groups[0])->ListTabs(),
            gfx::Range(0, 2));
}

// Drags a tab group by the header to a new position toward the left and presses
// escape to revert the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       RevertHeaderDragLeft) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({2, 3});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  TabGroupHeader* group_header = tab_strip->group_header(group);
  EXPECT_FALSE(group_header->dragging());
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(group_header)));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(group_header->dragging());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      browser()->window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
      false, false));

  StopAnimating(tab_strip);
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_FALSE(group_header->dragging());
  EXPECT_EQ("0 1 2 3", IDString(browser()->tab_strip_model()));
  std::vector<tab_groups::TabGroupId> groups =
      model->group_model()->ListTabGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(model->group_model()->GetTabGroup(groups[0])->ListTabs(),
            gfx::Range(2, 4));
}

namespace {

void PressEscapeWhileDetachedHeaderStep2(
    DetachToBrowserTabDragControllerTest* test) {
  // At this moment there should be a new browser window for the dragged tabs.
  EXPECT_EQ(2u, test->browser_list->size());
  Browser* new_browser = test->browser_list->get(1);
  std::vector<tab_groups::TabGroupId> new_browser_groups =
      new_browser->tab_strip_model()->group_model()->ListTabGroups();
  EXPECT_EQ(1u, new_browser_groups.size());
  EXPECT_EQ(0u, test->browser()
                    ->tab_strip_model()
                    ->group_model()
                    ->ListTabGroups()
                    .size());

  TabGroupHeader* new_group_header =
      GetTabStripForBrowser(new_browser)->group_header(new_browser_groups[0]);
  EXPECT_TRUE(new_group_header->dragging());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      new_browser->window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
      false, false));
}

}  // namespace

// Drags a tab group by the header and while detached presses escape to revert
// the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       RevertHeaderDragWhileDetached) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  AddTabsAndResetBrowser(browser(), 1);
  tab_groups::TabGroupId group = model->AddToNewGroup({0});
  StopAnimating(tab_strip);
  EnsureFocusToTabStrip(tab_strip);

  DragToDetachGroupAndNotify(
      tab_strip, base::BindOnce(&PressEscapeWhileDetachedHeaderStep2, this),
      group);

  EXPECT_FALSE(tab_strip->group_header(group)->dragging());

  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
  std::vector<tab_groups::TabGroupId> groups =
      model->group_model()->ListTabGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(model->group_model()->GetTabGroup(groups[0])->ListTabs(),
            gfx::Range(0, 1));
}

// Creates a browser with four tabs where the second and third tab is in a
// collapsed group. Drag the fourth tab to the left past the group header. The
// fourth tab should swap places with the collapsed group header.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragTabLeftPastCollapsedGroupHeader) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({1, 2});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(1, 3));
  EXPECT_FALSE(model->IsGroupCollapsed(group));
  tab_strip->ToggleTabGroupCollapsedState(group);
  StopAnimating(tab_strip);
  EXPECT_TRUE(model->IsGroupCollapsed(group));

  // Dragging the last tab to the left should cause it to swap places with the
  // collapsed group header.
  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(DragInputTo(
      test::GetLeftCenterInScreenCoordinates(tab_strip->tab_at(3))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("0 3 1 2", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(2, 4));
  EXPECT_TRUE(model->IsGroupCollapsed(group));
}

// Creates a browser with four tabs where the second and third tab is in a
// collapsed group. Drag the first tab to the right past the group header. The
// first tab should swap places with the collapsed group header.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragTabRightPastCollapsedGroupHeader) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({1, 2});
  StopAnimating(tab_strip);

  EXPECT_EQ(4, model->count());
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(1, 3));
  EXPECT_FALSE(model->IsGroupCollapsed(group));
  tab_strip->ToggleTabGroupCollapsedState(group);
  StopAnimating(tab_strip);
  EXPECT_TRUE(model->IsGroupCollapsed(group));

  // Dragging the first tab to the right should cause it to swap places with the
  // collapsed group header.
  ASSERT_TRUE(
      PressInput(test::GetLeftCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(
      test::GetRightCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);

  EXPECT_EQ("1 2 0 3", IDString(model));
  EXPECT_EQ(model->group_model()->GetTabGroup(group)->ListTabs(),
            gfx::Range(0, 2));
  EXPECT_TRUE(model->IsGroupCollapsed(group));
}

// Drags a tab group by the header and while detached presses escape to revert
// the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       RevertCollapsedHeaderDragWhileDetached) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  AddTabsAndResetBrowser(browser(), 1);
  tab_groups::TabGroupId group = model->AddToNewGroup({0});
  EXPECT_FALSE(model->IsGroupCollapsed(group));
  EnsureFocusToTabStrip(tab_strip);

  tab_strip->ToggleTabGroupCollapsedState(group);
  StopAnimating(tab_strip);
  EXPECT_TRUE(model->IsGroupCollapsed(group));

  DragToDetachGroupAndNotify(
      tab_strip, base::BindOnce(&PressEscapeWhileDetachedHeaderStep2, this),
      group);

  EXPECT_FALSE(tab_strip->group_header(group)->dragging());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
  std::vector<tab_groups::TabGroupId> groups =
      model->group_model()->ListTabGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(model->group_model()->GetTabGroup(groups[0])->ListTabs(),
            gfx::Range(0, 1));
  EXPECT_TRUE(tab_strip->IsGroupCollapsed(group));
}

// Creates a browser with four tabs. The first two tabs belong in Tab Group 1.
// Dragging the collapsed group header of Tab Group 1 will result in Tab Group 1
// expanding.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragCollapsedGroupHeaderExpandsGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  TabGroupModel* group_model = model->group_model();

  AddTabsAndResetBrowser(browser(), 3);
  tab_groups::TabGroupId group = model->AddToNewGroup({2, 3});
  ASSERT_FALSE(model->IsGroupCollapsed(group));
  tab_strip->ToggleTabGroupCollapsedState(group);
  StopAnimating(tab_strip);
  ASSERT_TRUE(model->IsGroupCollapsed(group));
  EnsureFocusToTabStrip(tab_strip);

  ASSERT_EQ(4, model->count());
  ASSERT_EQ(2u, group_model->GetTabGroup(group)->ListTabs().length());

  // Drag group1, this should expand the group.
  ASSERT_TRUE(
      PressInput(GetCenterInScreenCoordinates(tab_strip->group_header(group))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  ASSERT_TRUE(TabDragController::IsActive());
  EXPECT_FALSE(model->IsGroupCollapsed(group));
  ASSERT_TRUE(ReleaseInput());
  StopAnimating(tab_strip);
  EXPECT_FALSE(model->IsGroupCollapsed(group));
}

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
// Test is flaky on Mac and Linux: https://crbug.com/1167249
#define MAYBE_DragCollapsedGroupHeaderToSeparateWindow \
  DISABLED_DragCollapsedGroupHeaderToSeparateWindow
#else
#define MAYBE_DragCollapsedGroupHeaderToSeparateWindow \
  DragCollapsedGroupHeaderToSeparateWindow
#endif

// Creates two browsers, then drags a collapsed group from one to the other.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragCollapsedGroupHeaderToSeparateWindow) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();
  AddTabsAndResetBrowser(browser(), 2);
  tab_groups::TabGroupId group = model->AddToNewGroup({0, 1});
  EXPECT_FALSE(model->IsGroupCollapsed(group));
  tab_strip->ToggleTabGroupCollapsedState(group);
  StopAnimating(tab_strip);
  EXPECT_TRUE(model->IsGroupCollapsed(group));

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  TabStripModel* model2 = browser2->tab_strip_model();
  StopAnimating(tab_strip2);

  // Drag the group by its header into the second browser.
  DragToDetachGroupAndNotify(
      tab_strip,
      base::BindOnce(&DragToSeparateWindowStep2, this, tab_strip, tab_strip2),
      group);
  ASSERT_TRUE(ReleaseInput());

  // Expect the group to be in browser2, but with a new tab_groups::TabGroupId
  // and not collapsed.
  EXPECT_EQ("100 0 1", IDString(model2));
  std::vector<tab_groups::TabGroupId> browser2_groups =
      model2->group_model()->ListTabGroups();
  EXPECT_EQ(1u, browser2_groups.size());
  EXPECT_EQ(model2->group_model()->GetTabGroup(browser2_groups[0])->ListTabs(),
            gfx::Range(1, 3));
  ASSERT_FALSE(tab_strip->IsGroupCollapsed(browser2_groups[0]));
  EXPECT_EQ(browser2_groups[0], group);
}

// Detachable tabs are not supported for PWAs on Mac so these tests don't apply.
#if !BUILDFLAG(IS_MAC)
class DetachToBrowserTabDragControllerTestWithTabbedWebApp
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserTabDragControllerTestWithTabbedWebApp() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDesktopPWAsTabStrip}, {});
  }

  web_app::AppId InstallMockApp(bool add_home_tab) {
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = GURL("https://www.example.com");
    web_app_info->title = u"A tabbed web app";
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->display_override = {blink::mojom::DisplayMode::kTabbed};
    if (add_home_tab) {
      blink::Manifest::TabStrip manifest_tab_strip;
      manifest_tab_strip.home_tab = blink::Manifest::HomeTabParams();
      web_app_info->tab_strip = std::move(manifest_tab_strip);
    }

    return web_app::test::InstallWebApp(browser()->profile(),
                                        std::move(web_app_info));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tabbed web apps with the home tab cannot have detachable tabs.
// TODO(crbug.com/1381358): Enable this test for Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_HomeTabAddedToEveryWindow DISABLED_HomeTabAddedToEveryWindow
#else
#define MAYBE_HomeTabAddedToEveryWindow HomeTabAddedToEveryWindow
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestWithTabbedWebApp,
                       MAYBE_HomeTabAddedToEveryWindow) {
  // Install tabbed web app.
  web_app::AppId app_id = InstallMockApp(/*add_home_tab=*/true);
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_EQ(2u, browser_list->size());

  // Close normal browser since other code expects only 1 browser to start.
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, browser_list->size());

  SelectFirstBrowser();
  ASSERT_EQ(app_browser, browser());

  AddTabsAndResetBrowser(browser(), 1, GURL("https://www.example.com/newpage"));

  TabStrip* tab_strip = GetTabStripForBrowser(app_browser);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // Move to the second tab and drag it enough that it detaches.
  int tab_1_width = tab_strip->tab_at(1)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_1_width),
                   1);

  EXPECT_EQ(2u, browser_list->size());

  // Expect first window is left with just the home tab.
  TabStripModel* source_tab_strip_model =
      browser_list->get(0)->tab_strip_model();
  EXPECT_EQ(source_tab_strip_model->count(), 1);
  EXPECT_TRUE(source_tab_strip_model->IsTabPinned(0));
  EXPECT_EQ(source_tab_strip_model->GetWebContentsAt(0)->GetVisibleURL(),
            web_app::WebAppProvider::GetForTest(browser()->profile())
                ->registrar_unsafe()
                .GetAppStartUrl(app_id));

  // Expect the newly created window has the dragged tab and a home tab.
  TabStripModel* dest_tab_strip_model = browser_list->get(1)->tab_strip_model();
  EXPECT_EQ(dest_tab_strip_model->count(), 2);
  EXPECT_TRUE(dest_tab_strip_model->IsTabPinned(0));
  EXPECT_EQ(dest_tab_strip_model->GetWebContentsAt(0)->GetVisibleURL(),
            source_tab_strip_model->GetWebContentsAt(0)->GetVisibleURL());
  EXPECT_EQ(dest_tab_strip_model->GetWebContentsAt(1)->GetVisibleURL(),
            GURL("https://www.example.com/newpage"));
  EXPECT_EQ(dest_tab_strip_model->active_index(), 1);
}

// Home tab can't be detached.
// TODO(crbug.com/1381358): Enable this test for Linux and Lacros.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#define MAYBE_CantDragHomeTab DISABLED_CantDragHomeTab
#else
#define MAYBE_CantDragHomeTab CantDragHomeTab
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestWithTabbedWebApp,
                       MAYBE_CantDragHomeTab) {
  // Install tabbed web app.
  web_app::AppId app_id = InstallMockApp(/*add_home_tab=*/true);
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_EQ(2u, browser_list->size());

  // Close normal browser since other code expects only 1 browser to start.
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, browser_list->size());

  SelectFirstBrowser();
  ASSERT_EQ(app_browser, browser());

  AddTabsAndResetBrowser(browser(), 1, GURL("https://www.example.com/newpage"));

  TabStrip* tab_strip = GetTabStripForBrowser(app_browser);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // Try dragging the home tab enough that it would usually detach.
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center + gfx::Vector2d(0, GetDetachY(tab_strip)),
      base::BindLambdaForTesting([&]() {
        ASSERT_TRUE(TabDragController::IsActive());

        ASSERT_TRUE(ReleaseInput());

        // There should only be one browser window containing two tabs.
        EXPECT_EQ(1u, browser_list->size());
        EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
      })));
}

// Tabbed web apps without a home tab do not have home tab added.
// TODO(crbug.com/1381358): Enable this test for Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_NoHomeTab DISABLED_NoHomeTab
#else
#define MAYBE_NoHomeTab NoHomeTab
#endif
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestWithTabbedWebApp,
                       MAYBE_NoHomeTab) {
  // Install tabbed web app.
  web_app::AppId app_id = InstallMockApp(/*add_home_tab=*/false);
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_EQ(2u, browser_list->size());

  // Close normal browser since other code expects only 1 browser to start.
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, browser_list->size());

  SelectFirstBrowser();
  ASSERT_EQ(app_browser, browser());

  AddTabsAndResetBrowser(browser(), 1, GURL("https://www.example.com/newpage"));

  TabStrip* tab_strip = GetTabStripForBrowser(app_browser);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // Move to the second tab and drag it enough that it detaches.
  int tab_1_width = tab_strip->tab_at(1)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_1_width),
                   1);

  // Expect 2 app windows with 1 tab each.
  ASSERT_EQ(2u, browser_list->size());
  EXPECT_EQ(browser_list->get(0)->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser_list->get(1)->tab_strip_model()->count(), 1);
}
#endif  // !BUILDFLAG(IS_MAC)

class DetachToBrowserTabDragControllerTestWithScrollableTabStripEnabled
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserTabDragControllerTestWithScrollableTabStripEnabled() {
    scoped_feature_list_.InitWithFeatures({features::kScrollableTabStrip}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
// Disabling on macOS due to DCHECK crashes; see https://crbug.com/1183043.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    (BUILDFLAG(IS_MAC) && DCHECK_IS_ON())
#define MAYBE_DraggingRightExpandsTabStripSize \
  DISABLED_DraggingRightExpandsTabStripSize
#else
#define MAYBE_DraggingRightExpandsTabStripSize DraggingRightExpandsTabStripSize
#endif
// Creates a browser with two tabs and drags the rightmost tab. Given the
// browser window is large enough, the tabstrip should expand to accommodate
// this tab. Note: There must be at least two tabs because dragging a singular
// tab will drag the window.
// Disabled for Linux due to test dragging flakiness.
IN_PROC_BROWSER_TEST_P(
    DetachToBrowserTabDragControllerTestWithScrollableTabStripEnabled,
    MAYBE_DraggingRightExpandsTabStripSize) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  const TabStyle* tab_style = TabStyle::Get();
  // We must ensure that we set the bounds of the browser window such that it is
  // wide enough to allow the tab strip to expand to accommodate this tab.
  browser()->window()->SetBounds(
      gfx::Rect(0, 0, tab_style->GetStandardWidth() * 5, 400));

  const int tab_strip_width = tab_strip->width();
  const gfx::Point tab_1_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(1));
  ASSERT_TRUE(PressInput(tab_1_center));
  ASSERT_TRUE(DragInputTo(tab_1_center +
                          gfx::Vector2d(tab_style->GetStandardWidth(), 0)));
  BrowserView::GetBrowserViewForBrowser(browser())
      ->GetWidget()
      ->LayoutRootViewIfNecessary();
  EXPECT_EQ(tab_strip_width + tab_style->GetStandardWidth(),
            tab_strip->width());
  ASSERT_TRUE(ReleaseInput());
}

namespace {

// Invoked from the nested run loop.
void DragAllToSeparateWindowAndCancelStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* attached_tab_strip,
    TabStrip* target_tab_strip,
    const BrowserList* browser_list) {
  ASSERT_TRUE(attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  ASSERT_TRUE(
      test->DragInputToAsync(GetCenterInScreenCoordinates(target_tab_strip)));
}

}  // namespace

#if BUILDFLAG(IS_MAC) /* && defined(ARCH_CPU_ARM64) */
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
// These were flaking on all macs, so commented out ARCH_ above for
// crbug.com/1160917 too.
#define MAYBE_DragAllToSeparateWindowAndCancel \
  DISABLED_DragAllToSeparateWindowAndCancel
#else
// TODO(https://crbug.com/1163775): Flaky on Windows and Linux.
#define MAYBE_DragAllToSeparateWindowAndCancel \
  DISABLED_DragAllToSeparateWindowAndCancel
#endif

// Creates two browsers, selects all tabs in first, drags into second, then hits
// escape.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragAllToSeparateWindowAndCancel) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->ToggleSelectionAt(0);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragAllToSeparateWindowAndCancelStep2, this,
                                  tab_strip, tab_strip2, browser_list));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Cancel the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser2, ui::VKEY_ESCAPE, false, false, false, false));

  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  // browser() will have been destroyed, but browser2 should remain.
  ASSERT_EQ(1u, browser_list->size());

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

#if BUILDFLAG(IS_MAC) /* && defined(ARCH_CPU_ARM64) */
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
// These were flaking on all macs, so commented out ARCH_ above for
// crbug.com/1160917 too.
#define MAYBE_DragAllToSeparateWindowWithPinnedTabs \
  DISABLED_DragAllToSeparateWindowWithPinnedTabs
#else
// TODO(https://crbug.com/1163775): Flaky on Windows and Linux.
#define MAYBE_DragAllToSeparateWindowWithPinnedTabs \
  DISABLED_DragAllToSeparateWindowWithPinnedTabs
#endif

// Creates two browsers, selects all tabs in first, drags into second, then hits
// escape.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_DragAllToSeparateWindowWithPinnedTabs) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Open a second tab.
  AddTabsAndResetBrowser(browser(), 1);
  // Re-select the first one.
  browser()->tab_strip_model()->ToggleSelectionAt(0);

  // Create another browser.
  Browser* target_browser = CreateAnotherBrowserAndResize();
  TabStrip* target_tab_strip = GetTabStripForBrowser(target_browser);

  // Pin the tab in the target tabstrip.
  target_browser->tab_strip_model()->SetTabPinned(0, true);

  // Drag the selected tabs to |target_tab_strip|.
  DragTabAndNotify(
      tab_strip, base::BindOnce(&DragAllToSeparateWindowStep2, this, tab_strip,
                                target_tab_strip));

  // Should now be attached to |target_tab_strip|.
  ASSERT_TRUE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Drag to the trailing end of the tabstrip to ensure we're in a consistent
  // spot within the strip.
  StopAnimating(target_tab_strip);
  ASSERT_TRUE(
      DragInputTo(GetCenterInScreenCoordinates(target_tab_strip->tab_at(1))));

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(target_browser->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(target_browser));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(target_browser->window()->IsMaximized());
}

// Creates two browsers, drags from first into the second in such a way that
// no detaching should happen.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragDirectlyToSecondWindow) {
  // TODO(pkasting): Crashes when detaching browser.  https://crbug.com/918733
  if (input_source() == INPUT_SOURCE_TOUCH) {
    VLOG(1) << "Test is DISABLED for touch input.";
    return;
  }

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  const gfx::Rect initial_bounds(browser2->window()->GetBounds());

  // Place the first browser directly below the second in such a way that
  // dragging a tab upwards will drag it directly into the second browser's
  // tabstrip.
  const BrowserView* const browser_view2 =
      BrowserView::GetBrowserViewForBrowser(browser2);
  const gfx::Rect tabstrip_region2_bounds =
      browser_view2->frame()->GetBoundsForTabStripRegion(
          browser_view2->tab_strip_region_view()->GetMinimumSize());
  gfx::Rect bounds = initial_bounds;
  bounds.Offset(0, tabstrip_region2_bounds.bottom());
  browser()->window()->SetBounds(bounds);

  // Ensure the first browser is on top so clicks go to it.
  ui_test_utils::BrowserActivationWaiter activation_waiter(browser());
  browser()->window()->Activate();
  activation_waiter.WaitForActivation();

  // Move to the first tab and drag it to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  const views::View* tab = tab_strip2->tab_at(0);
  gfx::Point b2_location(GetCenterInScreenCoordinates(tab));
  b2_location.Offset(-tab->width() / 4, 0);
  ASSERT_TRUE(DragInputTo(b2_location));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Make sure that the window is still managed and not user moved.
  EXPECT_TRUE(IsWindowPositionManaged(browser2->window()->GetNativeWindow()));
  EXPECT_FALSE(HasUserChangedWindowPositionOrSize(
      browser2->window()->GetNativeWindow()));

  // Also make sure that the drag to window position has not changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser2->window()->GetBounds().ToString());
}

// Flaky. https://crbug.com/1176998
#if (BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)) || BUILDFLAG(IS_LINUX)
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
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
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&DragAllToSeparateWindowStep2,
                                             this, tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, browser_list->size());

  // Drag to the trailing end of the tabstrip to ensure we're in a consistent
  // spot within the strip.
  StopAnimating(tab_strip2);
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip2->tab_at(1))));

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));

  EXPECT_FALSE(GetIsDragged(browser2));

  // Remaining browser window should not be maximized
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

#if !BUILDFLAG(IS_MAC)
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

// Adds another tab, detaches into separate window, adds another tab and
// verifies the run loop ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       CancelOnNewTabWhenDragging) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  AddTabsAndResetBrowser(browser(), 1);

  // Move to the first tab and drag it enough so that it detaches.
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  ASSERT_TRUE(PressInput(tab_0_center));

  // Add another tab. This should trigger exiting the nested loop. Add at the
  // beginning to exercise past crash when model/tabstrip got out of sync.
  // crbug.com/474082
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center + gfx::Vector2d(0, GetDetachY(tab_strip)),
      base::BindOnce(&CancelOnNewTabWhenDraggingStep2, this, browser_list)));
  observer.Wait();

  // Should be two windows and not dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());
  for (auto* browser : *BrowserList::GetInstance()) {
    EXPECT_FALSE(GetIsDragged(browser));
    // Should not be maximized
    EXPECT_FALSE(browser->window()->IsMaximized());
  }
}
#endif

namespace {

TabStrip* GetAttachedTabstrip() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (TabDragController::IsAttachedTo(
            browser_view->tabstrip()->GetDragContext()))
      return browser_view->tabstrip();
  }
  return nullptr;
}

void DragWindowAndVerifyOffset(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* tab_strip,
                               int tab_index) {
  test::QuitDraggingObserver observer(tab_strip);
  // Move to the tab and drag it enough so that it detaches.
  const gfx::Point tab_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(tab_index));
  // The expected offset; the horizontal position should be relative to the
  // pressed tab. The vertical position should be relative to the window itself
  // since the top margin can be different between the existing browser and
  // the dragged one.
  const gfx::Vector2d press_offset(
      tab_center.x() - tab_strip->tab_at(tab_index)->GetBoundsInScreen().x(),
      tab_center.y() - tab_strip->GetWidget()->GetWindowBoundsInScreen().y());
  const gfx::Point initial_move =
      tab_center + gfx::Vector2d(0, GetDetachY(tab_strip));
  const gfx::Point second_move = initial_move + gfx::Vector2d(20, 20);
  ASSERT_TRUE(test->PressInput(tab_center));
  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      initial_move, base::BindLambdaForTesting([&]() {
        // Moves slightly to cause the actual dragging effect on the system and
        // makes sure the window is positioned correctly.
        ASSERT_TRUE(test->DragInputToNotifyWhenDone(
            second_move, base::BindLambdaForTesting([&]() {
              TabStrip* attached = GetAttachedTabstrip();
              // Same computation for drag offset. This operation drags a single
              // tab, so the target tab index should be always 0.
              gfx::Vector2d drag_offset(
                  second_move.x() -
                      attached->tab_at(0)->GetBoundsInScreen().x(),
                  second_move.y() -
                      attached->GetWidget()->GetWindowBoundsInScreen().y());
              EXPECT_EQ(press_offset, drag_offset);
              ASSERT_TRUE(test->ReleaseInput());
            })));
      })));
  observer.Wait();
}

}  // namespace

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
// TODO(mukai): enable this test on Windows and Linux.
// TODO(crbug.com/979013): flaky on Mac
#define MAYBE_OffsetForDraggingTab DISABLED_OffsetForDraggingTab
#else
#define MAYBE_OffsetForDraggingTab OffsetForDraggingTab
#endif

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       MAYBE_OffsetForDraggingTab) {
  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 0);
  ASSERT_FALSE(TabDragController::IsActive());
}

// TODO(960915): fix flakiness and re-enable this test on mac/linux.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DISABLED_OffsetForDraggingDetachedTab) {
  AddTabsAndResetBrowser(browser(), 1);

  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 1);
  ASSERT_FALSE(TabDragController::IsActive());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());

  // Both windows should be visible.
  EXPECT_TRUE(tab_strip->GetWidget()->IsVisible());
  EXPECT_TRUE(tab_strip2->GetWidget()->IsVisible());

  // Stops dragging.
  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Creates a browser with two tabs, maximizes it, drags the tab out.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragInMaximizedWindow) {
  AddTabsAndResetBrowser(browser(), 1);
  browser()->window()->Maximize();

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragInMaximizedWindowStep2, this, browser(),
                                  tab_strip, browser_list));

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

namespace {

void NewBrowserWindowStateStep2(DetachToBrowserTabDragControllerTest* test,
                                TabStrip* tab_strip) {
  // There should be two browser windows, including the newly created one for
  // the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // Get this new created window for the dragged tab.
  Browser* new_browser = test->browser_list->get(2);
  aura::Window* window = new_browser->window()->GetNativeWindow();
  EXPECT_NE(window->GetProperty(aura::client::kShowStateKey),
            ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            ui::SHOW_STATE_DEFAULT);

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Test that tab dragging can work on a browser window with its initial show
// state is MAXIMIZED.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       NewBrowserWindowState) {
  // Create a browser window whose initial show state is MAXIMIZED.
  Browser::CreateParams params(browser()->profile(), /*user_gesture=*/false);
  params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
  Browser* browser = Browser::Create(params);
  AddBlankTabAndShow(browser);
  TabStrip* tab_strip = GetTabStripForBrowser(browser);
  AddTabsAndResetBrowser(browser, 1);

  // Maximize the browser window.
  browser->window()->Maximize();
  EXPECT_EQ(browser->window()->GetNativeWindow()->GetProperty(
                aura::client::kShowStateKey),
            ui::SHOW_STATE_MAXIMIZED);

  // Drag it far enough that the first tab detaches.
  DragTabAndNotify(
      tab_strip, base::BindOnce(&NewBrowserWindowStateStep2, this, tab_strip));
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       OffsetForDraggingInMaximizedWindow) {
  AddTabsAndResetBrowser(browser(), 1);
  // Moves the browser window slightly to ensure that the browser's restored
  // bounds are different from the maximized bound's origin.
  browser()->window()->SetBounds(browser()->window()->GetBounds() +
                                 gfx::Vector2d(100, 50));
  browser()->window()->Maximize();

  DragWindowAndVerifyOffset(this, GetTabStripForBrowser(browser()), 0);
  ASSERT_FALSE(TabDragController::IsActive());
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
  DraggedWindowObserver(const DraggedWindowObserver&) = delete;
  DraggedWindowObserver& operator=(const DraggedWindowObserver&) = delete;
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

        // Change window's bounds to simulate what might happen in ash. If
        // TabDragController is still an observer of the dragged tabstrip's
        // widget, OnWidgetBoundsChanged() will calls into ContinueDragging()
        // to attach the dragged tabstrip into another browser, which might
        // cause chrome crash.
        window_->SetBounds(end_bounds_);
      }
    }
  }

 private:
  raw_ptr<DetachToBrowserTabDragControllerTest, ExperimentalAsh> test_;
  // The dragged window.
  raw_ptr<aura::Window, ExperimentalAsh> window_ = nullptr;
  // The bounds that |window_| will change to when the drag ends.
  gfx::Rect end_bounds_;
  // The position that the mouse/touch event will move to when the drag ends.
  gfx::Point end_point_;
};

void DoNotObserveDraggedWidgetAfterDragEndsStep2(
    DetachToBrowserTabDragControllerTest* test,
    DraggedWindowObserver* observer,
    TabStrip* attached_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Start observe the dragged window.
  observer->StartObserving(attached_tab_strip->GetWidget()->GetNativeWindow());

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Test that after the drag ends, TabDragController is no longer an observer of
// the dragged widget, so that if the bounds of the dragged widget change,
// TabDragController won't be put into dragging process again.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DoNotObserveDraggedWidgetAfterDragEnds) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  // Create an window observer to observe the dragged window.
  std::unique_ptr<DraggedWindowObserver> observer(new DraggedWindowObserver(
      this, test::GetWindowForTabStrip(tab_strip),
      tab_strip2->GetWidget()->GetNativeWindow()->bounds(),
      GetCenterInScreenCoordinates(tab_strip2)));

  // Drag the tab long enough so that it moves.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DoNotObserveDraggedWidgetAfterDragEndsStep2,
                                  this, observer.get(), tab_strip));

  // There should be still two browsers at this moment. |tab_strip| should not
  // be merged into |tab_strip2|.
  EXPECT_EQ(2u, browser_list->size());

  ASSERT_FALSE(TabDragController::IsActive());
}

namespace {

// Returns true if the web contents that's associated with |browser| is using
// fast resize.
bool WebContentsIsFastResized(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ContentsWebView* contents_web_view =
      static_cast<ContentsWebView*>(browser_view->GetContentsView());
  return contents_web_view->holder()->fast_resize();
}

void FastResizeDuringDraggingStep2(DetachToBrowserTabDragControllerTest* test,
                                   TabStrip* not_attached_tab_strip,
                                   TabStrip* target_tab_strip) {
  // There should be three browser windows, including the newly created one for
  // the dragged tab.
  EXPECT_EQ(3u, test->browser_list->size());

  // TODO(crbug.com/1110266): Remove explicit OS_CHROMEOS check once OS_LINUX
  // CrOS cleanup is done.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // Get this new created window for the drag. It should have fast resize set.
  Browser* new_browser = test->browser_list->get(2);
  EXPECT_TRUE(WebContentsIsFastResized(new_browser));
  // The source window should also have fast resize set.
  EXPECT_TRUE(WebContentsIsFastResized(test->browser()));
#endif

  // Now drag to target_tab_strip.
  ASSERT_TRUE(
      test->DragInputTo(GetCenterInScreenCoordinates(target_tab_strip)));

  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Tests that we use fast resize to resize the web contents of the dragged
// window and the source window during tab dragging process, and don't use fast
// resize after tab dragging ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       FastResizeDuringDragging) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  AddTabsAndResetBrowser(browser(), 1);

  // Create another browser.
  Browser* browser2 = CreateAnotherBrowserAndResize();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  EXPECT_EQ(2u, browser_list->size());

  EXPECT_FALSE(WebContentsIsFastResized(browser()));
  EXPECT_FALSE(WebContentsIsFastResized(browser2));

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip, base::BindOnce(&FastResizeDuringDraggingStep2,
                                             this, tab_strip, tab_strip2));

  EXPECT_FALSE(WebContentsIsFastResized(browser()));
  EXPECT_FALSE(WebContentsIsFastResized(browser2));
}

class DetachToBrowserTabDragControllerTestWithTabbedSystemApp
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserTabDragControllerTestWithTabbedSystemApp()
      : test_system_web_app_installation_(
            ash::TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp()) {}

  web_app::AppId InstallMockApp() {
    test_system_web_app_installation_->WaitForAppInstall();
    return test_system_web_app_installation_->GetAppId();
  }

  Browser* LaunchWebAppBrowser(web_app::AppId app_id) {
    return web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  }

  const GURL& GetAppUrl() {
    return test_system_web_app_installation_->GetAppUrl();
  }

 private:
  std::unique_ptr<ash::TestSystemWebAppInstallation>
      test_system_web_app_installation_;
};

// Move tab from TYPE_APP Browser to create new TYPE_APP Browser.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestWithTabbedSystemApp,
                       DragAppToOwnWindow) {
  // Install and get a tabbed system app.
  web_app::AppId tabbed_app_id = InstallMockApp();
  Browser* app_browser = LaunchWebAppBrowser(tabbed_app_id);
  ASSERT_EQ(2u, browser_list->size());

  // Close normal browser since other code expects only 1 browser to start.
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, browser_list->size());
  SelectFirstBrowser();
  ASSERT_EQ(app_browser, browser());
  EXPECT_EQ(Browser::Type::TYPE_APP, browser_list->get(0)->type());
  AddTabsAndResetBrowser(browser(), 1, GetAppUrl());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  int tab_0_width = tab_strip->tab_at(0)->width();
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DetachToBrowserTabDragControllerTest::
                                      ReleaseInputAfterWindowDetached,
                                  base::Unretained(this), tab_0_width));

  // New browser should be TYPE_APP.
  ASSERT_EQ(2u, browser_list->size());
  EXPECT_EQ(Browser::Type::TYPE_APP, browser_list->get(1)->type());
}

// Move tab from TYPE_APP Browser to another TYPE_APP Browser.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestWithTabbedSystemApp,
                       DragAppToAppWindow) {
  // Install and get 2 browsers with tabbed system app.
  web_app::AppId tabbed_app_id = InstallMockApp();
  Browser* app_browser1 = LaunchWebAppBrowser(tabbed_app_id);
  Browser* app_browser2 = LaunchWebAppBrowser(tabbed_app_id);
  ASSERT_EQ(3u, browser_list->size());
  ResetIDs(app_browser2->tab_strip_model(), 100);
  Resize(app_browser1, app_browser2);

  // Close normal browser since other code expects only 1 browser to start.
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(2u, browser_list->size());
  SelectFirstBrowser();
  ASSERT_EQ(app_browser1, browser());

  AddTabsAndResetBrowser(browser(), 1, GetAppUrl());
  TabStrip* tab_strip1 = GetTabStripForBrowser(app_browser1);
  TabStrip* tab_strip2 = GetTabStripForBrowser(app_browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip1, base::BindOnce(&DragToSeparateWindowStep2, this,
                                              tab_strip1, tab_strip2));

  // Should now be attached to tab_strip2.
  // Release mouse or touch, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  EXPECT_EQ("100 0", IDString(app_browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(app_browser1->tab_strip_model()));
}

// Subclass of DetachToBrowserTabDragControllerTest that
// creates multiple displays.
class DetachToBrowserInSeparateDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayTabDragControllerTest() {}
  DetachToBrowserInSeparateDisplayTabDragControllerTest(
      const DetachToBrowserInSeparateDisplayTabDragControllerTest&) = delete;
  DetachToBrowserInSeparateDisplayTabDragControllerTest& operator=(
      const DetachToBrowserInSeparateDisplayTabDragControllerTest&) = delete;
  virtual ~DetachToBrowserInSeparateDisplayTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-800x600,800+0-800x600");
  }
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
      target_point,
      base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep3,
                     test)));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragSingleTabToSeparateWindowInSecondDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  // Then drag it to the final destination on the second screen.
  display::Screen* const screen = display::Screen::GetScreen();
  display::Display second_display = ui_test_utils::GetSecondaryDisplay(screen);
  const gfx::Point start = GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  ASSERT_FALSE(second_display.bounds().Contains(start));
  const gfx::Point target(second_display.bounds().x() + 1,
                          start.y() + GetDetachY(tab_strip));
  ASSERT_TRUE(second_display.bounds().Contains(target));

  // TODO(crbug.com/990589): Unit tests should be able to simulate mouse input
  // without having to call |CursorManager::SetDisplay|.
  ash::Shell::Get()->cursor_manager()->SetDisplay(second_display);
  DragTabAndNotify(
      tab_strip,
      base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep2, this,
                     target));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  // This other browser should be on the second screen (with mouse drag)
  // With the touch input the browser cannot be dragged from one screen
  // to another and the window stays on the first screen.
  if (input_source() == INPUT_SOURCE_MOUSE) {
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
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->GetDragContext()->IsDragSessionActive());
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
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move the second browser to the second display.
  display::Screen* screen = display::Screen::GetScreen();
  Display second_display = ui_test_utils::GetSecondaryDisplay(screen);
  browser2->window()->SetBounds(second_display.work_area());
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Both windows should not be maximized
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// Crashes on ChromeOS. crbug.com/1003288
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DragBrowserWindowWhenMajorityOfBoundsInSecondDisplay \
  DISABLED_CDragBrowserWindowWhenMajorityOfBoundsInSecondDisplay
#else
#define MAYBE_DragBrowserWindowWhenMajorityOfBoundsInSecondDisplay \
  DragBrowserWindowWhenMajorityOfBoundsInSecondDisplay
#endif
IN_PROC_BROWSER_TEST_P(
    DetachToBrowserInSeparateDisplayTabDragControllerTest,
    MAYBE_DragBrowserWindowWhenMajorityOfBoundsInSecondDisplay) {
  // Set the browser's window bounds such that the majority of its bounds
  // resides in the second display.
  const std::pair<Display, Display> displays =
      GetDisplays(display::Screen::GetScreen());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  {
    // Moves the browser window through dragging so that the majority of its
    // bounds are in the secondary display but it's still be in the primary
    // display. Do not use SetBounds() or related, it may move the browser
    // window to the secondary display in some configurations like Mash.
    int target_x = displays.first.bounds().right() -
                   browser()->window()->GetBounds().width() / 2 + 20;
    const gfx::Point target_point =
        GetCenterInScreenCoordinates(tab_strip->tab_at(0)) +
        gfx::Vector2d(target_x - browser()->window()->GetBounds().x(),
                      GetDetachY(tab_strip));
    DragTabAndNotify(
        tab_strip,
        base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep2, this,
                       target_point));
    StopAnimating(tab_strip);
  }
  EXPECT_EQ(displays.first.id(),
            browser()->window()->GetNativeWindow()->GetHost()->GetDisplayId());

  // Start dragging the window by the tab strip, and move it only to the edge
  // of the first display. Expect at that point mouse would warp and the window
  // will therefore reside in the second display when mouse is released.
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  const int offset_x = tab_0_center.x() - browser()->window()->GetBounds().x();
  const int detach_y = tab_0_center.y() + GetDetachY(tab_strip);
  const int first_display_warp_edge_x = displays.first.bounds().right() - 1;
  const gfx::Point warped_point(displays.second.bounds().x() + 1, detach_y);

  DragTabAndNotify(
      tab_strip, base::BindLambdaForTesting([&]() {
        // This makes another event on the warped location because the test
        // system does not create it automatically as the result of pointer
        // warp.
        ASSERT_TRUE(DragInputToNotifyWhenDone(
            gfx::Point(first_display_warp_edge_x, detach_y),
            base::BindOnce(&DragSingleTabToSeparateWindowInSecondDisplayStep2,
                           this, warped_point)));
      }));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should only be a single browser.
  ASSERT_EQ(1u, browser_list->size());
  ASSERT_EQ(browser(), browser_list->get(0));
  ASSERT_TRUE(browser()->window()->IsActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());

  // Browser now resides in display 2.
  EXPECT_EQ(warped_point.x() - offset_x, browser()->window()->GetBounds().x());
  EXPECT_EQ(displays.second.id(),
            browser()->window()->GetNativeWindow()->GetHost()->GetDisplayId());
}

// Drags from browser to another browser on a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragTabToWindowOnSecondDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
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
  // It's possible the window will not fit in half the screen, in which case we
  // will position the windows as well as we can.
  work_area.set_x(browser()->window()->GetBounds().right());
  // Sanity check: second browser should still be on the second display.
  ASSERT_LT(work_area.x(), second_display.work_area().right());
  browser2->window()->SetBounds(work_area);
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());
  EXPECT_EQ(
      second_display.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Sanity check: make sure the target position is also within in the screen
  // bounds:
  ASSERT_LT(GetCenterInScreenCoordinates(tab_strip2->tab_at(0)).x(),
            second_display.work_area().right());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
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
  AddTabsAndResetBrowser(browser(), 1);
  browser()->window()->Maximize();
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser on the second display.
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(2, screen->GetNumDisplays());
  const std::pair<Display, Display> displays = GetDisplays(screen);
  gfx::Rect work_area = displays.second.work_area();
  work_area.Inset(gfx::Insets::TLBR(20, 20, 60, 20));
  Browser::CreateParams params(browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_NORMAL;
  params.initial_bounds = work_area;
  Browser* browser2 = Browser::Create(params);
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
  EXPECT_EQ(2, tab_strip->GetTabCount());
  EXPECT_EQ(1, tab_strip2->GetTabCount());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());

  // tab should have moved
  EXPECT_EQ(1, tab_strip->GetTabCount());
  EXPECT_EQ(2, tab_strip2->GetTabCount());

  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // Source browser should still be maximized, target should not
  EXPECT_TRUE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser2->window()->IsMaximized());
}

// Drags from a restored browser to an immersive fullscreen browser on a
// second display and releases input.
// TODO(pkasting) https://crbug.com/910782 Hangs.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DISABLED_DragTabToImmersiveBrowserOnSeparateDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move the second browser to the second display.
  display::Screen* screen = display::Screen::GetScreen();
  const std::pair<Display, Display> displays = GetDisplays(screen);
  browser2->window()->SetBounds(displays.second.work_area());
  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser2->window()->GetNativeWindow())
          .id());

  // Put the second browser into immersive fullscreen.
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  ImmersiveModeController* immersive_controller2 =
      browser_view2->immersive_mode_controller();
  chromeos::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerChromeos*>(immersive_controller2)
          ->controller())
      .SetupForTest();
  chrome::ToggleFullscreenMode(browser2);
  // For MD, the browser's top chrome is completely offscreen, with tabstrip
  // visible.
  ASSERT_TRUE(immersive_controller2->IsEnabled());
  ASSERT_FALSE(immersive_controller2->IsRevealed());
  ASSERT_TRUE(tab_strip2->GetVisible());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&DragTabToWindowInSeparateDisplayStep2, this,
                                  tab_strip, tab_strip2));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // browser2's top chrome should be revealed and the tab strip should be
  // at normal height while user is dragging tabs_strip2's tabs.
  ASSERT_TRUE(immersive_controller2->IsRevealed());
  ASSERT_TRUE(tab_strip2->GetVisible());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
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
  EXPECT_TRUE(tab_strip2->GetVisible());
}

// Subclass of DetachToBrowserTabDragControllerTest that
// creates multiple displays with different device scale factors.
class DifferentDeviceScaleFactorDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DifferentDeviceScaleFactorDisplayTabDragControllerTest() {}
  DifferentDeviceScaleFactorDisplayTabDragControllerTest(
      const DifferentDeviceScaleFactorDisplayTabDragControllerTest&) = delete;
  DifferentDeviceScaleFactorDisplayTabDragControllerTest& operator=(
      const DifferentDeviceScaleFactorDisplayTabDragControllerTest&) = delete;
  virtual ~DifferentDeviceScaleFactorDisplayTabDragControllerTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "800x600,800+0-800x600*2");
  }

  float GetCursorDeviceScaleFactor() const {
    auto* cursor_client = aura::client::GetCursorClient(
        browser()->window()->GetNativeWindow()->GetRootWindow());
    const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
    return cursor_shape_client.GetCursorData(cursor_client->GetCursor())
        ->scale_factor;
  }
};

namespace {

// The points where a tab is dragged in CursorDeviceScaleFactorStep.
constexpr gfx::Point kDragPoints[] = {
    {300, 200}, {399, 200}, {500, 200}, {400, 200}, {300, 200},
};

// The expected device scale factors after the cursor is moved to the
// corresponding kDragPoints in CursorDeviceScaleFactorStep.
constexpr float kDeviceScaleFactorExpectations[] = {
    1.0f, 1.0f, 2.0f, 2.0f, 1.0f,
};

static_assert(
    std::size(kDragPoints) == std::size(kDeviceScaleFactorExpectations),
    "kDragPoints and kDeviceScaleFactorExpectations must have the same "
    "number of elements");

// Drags tab to |kDragPoints[index]|, then calls the next step function.
void CursorDeviceScaleFactorStep(
    DifferentDeviceScaleFactorDisplayTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    size_t index) {
  SCOPED_TRACE(index);
  ASSERT_FALSE(not_attached_tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  if (index > 0) {
    EXPECT_EQ(kDragPoints[index - 1],
              aura::Env::GetInstance()->last_mouse_location());
    EXPECT_EQ(kDeviceScaleFactorExpectations[index - 1],
              test->GetCursorDeviceScaleFactor());
  }

  if (index < std::size(kDragPoints)) {
    ASSERT_TRUE(test->DragInputToNotifyWhenDone(
        kDragPoints[index], base::BindOnce(&CursorDeviceScaleFactorStep, test,
                                           not_attached_tab_strip, index + 1)));
  } else {
    // Finishes a series of CursorDeviceScaleFactorStep calls and ends drag.
    ASSERT_TRUE(
        ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  }
}

}  // namespace

// Verifies cursor's device scale factor is updated when a tab is moved across
// displays with different device scale factors (http://crbug.com/154183).
// TODO(pkasting): In interactive_ui_tests, scale factor never changes to 2.
// https://crbug.com/918731
// TODO(pkasting): In non_single_process_mash_interactive_ui_tests, pointer is
// warped during the drag (which results in changing to scale factor 2 early),
// and scale factor doesn't change back to 1 at the end.
// https://crbug.com/918732
IN_PROC_BROWSER_TEST_P(DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                       DISABLED_CursorDeviceScaleFactor) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move the second browser to the second display.
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  // Move to the first tab and drag it enough so that it detaches.
  DragTabAndNotify(tab_strip, base::BindOnce(&CursorDeviceScaleFactorStep, this,
                                             tab_strip, 0));
}

class DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest() {}
  DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest(
      const DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest&) =
      delete;
  DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest& operator=(
      const DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-800x600,800+0-800x600");
  }
};

namespace {

// Invoked from the nested run loop.
void CancelDragTabToWindowInSeparateDisplayStep3(
    TabStrip* tab_strip,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Switching display mode should cancel the drag operation.
  ash::ShellTestApi().AddRemoveDisplay();
}

// Invoked from the nested run loop.
void CancelDragTabToWindowInSeparateDisplayStep2(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest* test,
    TabStrip* tab_strip,
    Display current_display,
    gfx::Point final_destination,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  Browser* new_browser = browser_list->get(1);
  EXPECT_EQ(
      current_display.id(),
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(new_browser->window()->GetNativeWindow())
          .id());

  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      final_destination,
      base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep3, tab_strip,
                     browser_list)));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_P(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn2ndDisplay) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Move the second browser to the second display.
  const std::pair<Display, Display> displays =
      GetDisplays(display::Screen::GetScreen());
  gfx::Point final_destination = displays.second.work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep2,
                                  this, tab_strip, displays.first,
                                  final_destination, browser_list));

  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
}

// Drags from browser from a second display to primary and releases input.
IN_PROC_BROWSER_TEST_P(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn1stDisplay) {
  display::Screen* screen = display::Screen::GetScreen();
  const std::pair<Display, Display> displays = GetDisplays(screen);

  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
  EXPECT_EQ(
      displays.first.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());

  browser()->window()->SetBounds(displays.second.work_area());
  EXPECT_EQ(
      displays.second.id(),
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .id());

  // Move the second browser to the display.
  gfx::Point final_destination = displays.first.work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  DragTabAndNotify(tab_strip,
                   base::BindOnce(&CancelDragTabToWindowInSeparateDisplayStep2,
                                  this, tab_strip, displays.second,
                                  final_destination, browser_list));

  ASSERT_EQ(1u, browser_list->size());
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
}

// Subclass of DetachToBrowserTabDragControllerTest that runs tests only with
// touch input.
class DetachToBrowserTabDragControllerTestTouch
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserTabDragControllerTestTouch() {}
  DetachToBrowserTabDragControllerTestTouch(
      const DetachToBrowserTabDragControllerTestTouch&) = delete;
  DetachToBrowserTabDragControllerTestTouch& operator=(
      const DetachToBrowserTabDragControllerTestTouch&) = delete;
  virtual ~DetachToBrowserTabDragControllerTestTouch() {}

  void TearDown() override {
    ui::SetEventTickClockForTesting(nullptr);
    clock_.reset();
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> clock_;
};

namespace {
void PressSecondFingerWhileDetachedStep3(
    DetachToBrowserTabDragControllerTest* test) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());
  ASSERT_TRUE(test->browser_list->get(1)->window()->IsActive());

  ASSERT_TRUE(test->ReleaseInput());
  ASSERT_TRUE(test->ReleaseInput(1));
}

void PressSecondFingerWhileDetachedStep2(
    DetachToBrowserTabDragControllerTest* test,
    const gfx::Point& target_point) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, test->browser_list->size());
  ASSERT_TRUE(test->browser_list->get(1)->window()->IsActive());

  // Continue dragging after adding a second finger.
  ASSERT_TRUE(test->PressInput(gfx::Point(), 1));
  ASSERT_TRUE(test->DragInputToNotifyWhenDone(
      target_point,
      base::BindOnce(&PressSecondFingerWhileDetachedStep3, test)));
}

}  // namespace

// Detaches a tab and while detached presses a second finger.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       PressSecondFingerWhileDetached) {
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Move to the first tab and drag it enough so that it detaches. Drag it
  // slightly more horizontally so that it does not generate a swipe down
  // gesture that minimizes the detached browser window.
  const int touch_move_delta = GetDetachY(tab_strip);
  const gfx::Point target = GetCenterInScreenCoordinates(tab_strip->tab_at(0)) +
                            gfx::Vector2d(0, 2 * touch_move_delta);
  DragTabAndNotify(
      tab_strip,
      base::BindOnce(&PressSecondFingerWhileDetachedStep2, this, target), 0,
      touch_move_delta + 5);

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->GetDragContext()->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       LeftSnapShouldntCauseMergeAtEnd) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  AddTabsAndResetBrowser(browser(), 1);

  // Set the last mouse location at the center of tab 0. This shouldn't affect
  // the touch behavior below. See https://crbug.com/914527#c1 for the details
  // of how this can affect the result.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  base::RunLoop run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(tab_0_center.x(), tab_0_center.y(),
                                           run_loop.QuitClosure());
  run_loop.Run();

  // Drag the tab 1 to left-snapping.
  DragTabAndNotify(
      tab_strip, base::BindLambdaForTesting([&]() {
        const gfx::Rect display_bounds =
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
        const gfx::Point target(display_bounds.x(),
                                display_bounds.CenterPoint().y());
        ASSERT_TRUE(
            DragInputToNotifyWhenDone(target, base::BindLambdaForTesting([&]() {
                                        ASSERT_TRUE(ReleaseInput());
                                      })));
      }),
      1);

  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       FlingDownAtEndOfDrag) {
  // Reduce the minimum fling velocity for this specific test case to cause the
  // fling-down gesture in the middle of tab-dragging. This should end up with
  // minimizing the window. See https://crbug.com/902897 for the details.
  ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(1);

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  test::QuitDraggingObserver observer(tab_strip);
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  const gfx::Vector2d detach(0, GetDetachY(tab_strip));
  clock_ = std::make_unique<base::SimpleTestTickClock>();
  clock_->SetNowTicks(base::TimeTicks::Now());
  ui::SetEventTickClockForTesting(clock_.get());
  ASSERT_TRUE(PressInput(tab_0_center));
  clock_->Advance(base::Milliseconds(5));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center + detach, base::BindLambdaForTesting([&]() {
        // Drag down again; this should cause a fling-down event.
        clock_->Advance(base::Milliseconds(5));
        ASSERT_TRUE(DragInputToNotifyWhenDone(
            tab_0_center + detach + detach, base::BindLambdaForTesting([&]() {
              clock_->Advance(base::Milliseconds(5));
              ASSERT_TRUE(ReleaseInput());
            })));
      })));
  observer.Wait();

  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_TRUE(browser()->window()->IsMinimized());
  EXPECT_FALSE(browser()->window()->IsVisible());
}

IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTestTouch,
                       FlingOnStartingDrag) {
  ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(1);
  AddTabsAndResetBrowser(browser(), 1);
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  const gfx::Point tab_0_center =
      GetCenterInScreenCoordinates(tab_strip->tab_at(0));
  const gfx::Vector2d detach(0, GetDetachY(tab_strip));

  // Sends events to the server without waiting for its reply, which will cause
  // extra touch events before PerformWindowMove starts handling events.
  test::QuitDraggingObserver observer(tab_strip);
  clock_ = std::make_unique<base::SimpleTestTickClock>();
  clock_->SetNowTicks(base::TimeTicks::Now());
  ui::SetEventTickClockForTesting(clock_.get());
  ASSERT_TRUE(PressInput(tab_0_center));
  clock_->Advance(base::Milliseconds(5));
  ASSERT_TRUE(DragInputToAsync(tab_0_center + detach));
  clock_->Advance(base::Milliseconds(5));
  ASSERT_TRUE(DragInputToAsync(tab_0_center + detach + detach));
  clock_->Advance(base::Milliseconds(2));
  ASSERT_TRUE(ReleaseInput());
  observer.Wait();

  ASSERT_FALSE(tab_strip->GetDragContext()->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ(2u, browser_list->size());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

class SelectTabDuringDragObserver : public TabStripModelObserver {
 public:
  SelectTabDuringDragObserver() = default;
  ~SelectTabDuringDragObserver() override = default;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kMoved)
      return;
    const TabStripModelChange::Move* move = change.GetMove();
    int index_to_select = move->to_index == 0 ? 1 : 0;
    tab_strip_model->ToggleSelectionAt(index_to_select);
  }
};

}  // namespace

// Bug fix for crbug.com/1196309. Don't change tab selection while dragging.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       SelectTabDuringDrag) {
  TabStripModel* model = browser()->tab_strip_model();
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  SelectTabDuringDragObserver observer;
  model->AddObserver(&observer);

  AddTabsAndResetBrowser(browser(), 1);
  ASSERT_EQ(2, model->count());

  ASSERT_TRUE(PressInput(GetCenterInScreenCoordinates(tab_strip->tab_at(0))));
  ASSERT_TRUE(DragInputTo(GetCenterInScreenCoordinates(tab_strip->tab_at(1))));
  {
    gfx::Rect tab_bounds = tab_strip->tab_at(1)->GetLocalBounds();
    views::View::ConvertRectToScreen(tab_strip->tab_at(1), &tab_bounds);
    ASSERT_TRUE(DragInputTo(tab_bounds.right_center()));
  }
  ASSERT_TRUE(ReleaseInput());
}

#if BUILDFLAG(IS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(
    TabDragging,
    DetachToBrowserTabDragControllerTest,
    ::testing::Combine(::testing::Bool(), ::testing::Values("mouse", "touch")));
INSTANTIATE_TEST_SUITE_P(
    TabDragging,
    DetachToBrowserTabDragControllerTestWithScrollableTabStripEnabled,
    ::testing::Combine(::testing::Bool(), ::testing::Values("mouse", "touch")));
#else
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserTabDragControllerTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values("mouse")));
INSTANTIATE_TEST_SUITE_P(
    TabDragging,
    DetachToBrowserTabDragControllerTestWithScrollableTabStripEnabled,
    ::testing::Combine(::testing::Bool(), ::testing::Values("mouse")));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserInSeparateDisplayTabDragControllerTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values("mouse")));
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values("mouse")));
INSTANTIATE_TEST_SUITE_P(
    TabDragging,
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    ::testing::Combine(::testing::Bool(), ::testing::Values("mouse")));
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserTabDragControllerTestTouch,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values("touch")));
INSTANTIATE_TEST_SUITE_P(
    TabDragging,
    DetachToBrowserTabDragControllerTestWithTabbedSystemApp,
    ::testing::Combine(::testing::Bool(), ::testing::Values("mouse", "touch")));
#endif

#if BUILDFLAG(IS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(
    TabDragging,
    DetachToBrowserTabDragControllerTestWithTabbedWebApp,
    ::testing::Combine(::testing::Bool(), ::testing::Values("mouse", "touch")));
#elif !BUILDFLAG(IS_MAC)
INSTANTIATE_TEST_SUITE_P(TabDragging,
                         DetachToBrowserTabDragControllerTestWithTabbedWebApp,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values("mouse")));
#endif
