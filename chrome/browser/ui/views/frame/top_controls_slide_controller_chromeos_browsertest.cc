// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_controls_slide_controller.h"

#include <memory>
#include <numeric>
#include <tuple>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/safe_sprintf.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/math_util.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace {

using ::ash::AccessibilityManager;

enum class TopChromeShownState {
  kFullyShown,
  kFullyHidden,
};

enum class ScrollDirection {
  kUp,
  kDown,
};

// Checks that the translation part of the two given transforms are equal.
void CompareTranslations(const gfx::Transform& t1, const gfx::Transform& t2) {
  const gfx::Vector2dF t1_translation = t1.To2dTranslation();
  const gfx::Vector2dF t2_translation = t2.To2dTranslation();
  EXPECT_FLOAT_EQ(t1_translation.x(), t2_translation.x());
  EXPECT_FLOAT_EQ(t1_translation.y(), t2_translation.y());
}

content::RenderWidgetHost* GetRenderWidgetHost(content::WebContents* contents) {
  EXPECT_TRUE(contents);
  return contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
}

void SynchronizeBrowserWithRenderer(content::WebContents* contents) {
  content::MainThreadFrameObserver frame_observer(
      GetRenderWidgetHost(contents));
  frame_observer.Wait();
}

// A test view that will be added as a child to the BrowserView to verify how
// many times it's laid out while sliding is in progress.
class LayoutTestView : public views::View {
  METADATA_HEADER(LayoutTestView, views::View)

 public:
  explicit LayoutTestView(BrowserView* parent) {
    DCHECK(parent);
    parent->AddChildView(this);
    parent->GetWidget()->LayoutRootViewIfNecessary();
    layout_count_ = 0;
  }
  ~LayoutTestView() override = default;
  LayoutTestView(const LayoutTestView&) = delete;
  LayoutTestView& operator=(const LayoutTestView&) = delete;

  int layout_count() const { return layout_count_; }

  void Reset() {
    InvalidateLayout();
    layout_count_ = 0;
  }

  // views::View:
  void Layout(PassKey) override { ++layout_count_; }

 private:
  int layout_count_ = 0;
};

BEGIN_METADATA(LayoutTestView)
END_METADATA

class TestControllerObserver {
 public:
  virtual void OnShownRatioChanged(float shown_ratio) = 0;
  virtual void OnGestureScrollInProgressChanged(bool in_progress) = 0;

 protected:
  virtual ~TestControllerObserver() = default;
};

// Defines a wrapper around the real TopControlsSlideControllerChromeOS which
// will be injected in the BrowserView. This is used to intercept the calls to
// the real controller here in the tests witout affecting the production code.
// An object of this class owns the instance of the real controller, and itself
// is owned by the BrowserView (See
// BrowserView::InjectTopControlsSlideControllerForTesting()).
class TestController : public TopControlsSlideController {
 public:
  explicit TestController(
      std::unique_ptr<TopControlsSlideController> real_controller)
      : real_controller_(std::move(real_controller)) {
    DCHECK(real_controller_);
  }

  TestController(const TestController&) = delete;
  TestController& operator=(const TestController&) = delete;

  ~TestController() override = default;

  void AddObserver(TestControllerObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(TestControllerObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // TopControlsSlideController:
  bool IsEnabled() const override { return real_controller_->IsEnabled(); }

  float GetShownRatio() const override {
    return real_controller_->GetShownRatio();
  }

  void SetShownRatio(content::WebContents* contents, float ratio) override {
    real_controller_->SetShownRatio(contents, ratio);
    observers_.Notify(&TestControllerObserver::OnShownRatioChanged, ratio);
  }

  void OnBrowserFullscreenStateWillChange(bool new_fullscreen_state) override {
    real_controller_->OnBrowserFullscreenStateWillChange(new_fullscreen_state);
  }

  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override {
    return real_controller_->DoBrowserControlsShrinkRendererSize(contents);
  }

  void SetTopControlsGestureScrollInProgress(bool in_progress) override {
    real_controller_->SetTopControlsGestureScrollInProgress(in_progress);
    observers_.Notify(&TestControllerObserver::OnGestureScrollInProgressChanged,
                      in_progress);
  }

  bool IsTopControlsGestureScrollInProgress() const override {
    return real_controller_->IsTopControlsGestureScrollInProgress();
  }

  bool IsTopControlsSlidingInProgress() const override {
    return real_controller_->IsTopControlsSlidingInProgress();
  }

 private:
  std::unique_ptr<TopControlsSlideController> real_controller_;

  base::ObserverList<TestControllerObserver>::Unchecked observers_;
};

// Waits for a given terminal value (1.f or 0.f) of the browser top controls
// shown ratio on a given browser window.
class TopControlsShownRatioWaiter : public TestControllerObserver {
 public:
  explicit TopControlsShownRatioWaiter(TestController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }

  TopControlsShownRatioWaiter(const TopControlsShownRatioWaiter&) = delete;
  TopControlsShownRatioWaiter& operator=(const TopControlsShownRatioWaiter&) =
      delete;

  ~TopControlsShownRatioWaiter() override { controller_->RemoveObserver(this); }

  // TestControllerObserver:
  void OnShownRatioChanged(float shown_ratio) override { CheckRatio(); }

  void OnGestureScrollInProgressChanged(bool in_progress) override {
    CheckRatio();
  }

  void WaitForRatio(float ratio) {
    DCHECK(ratio == 1.f || ratio == 0.f) << "Should only be used to wait for "
                                            "terminal values of the shown "
                                            "ratio.";

    waiting_for_shown_ratio_ = ratio;
    if (CheckRatio())
      return;

    // Use kNestableTasksAllowed to make it possible to wait inside a posted
    // task.
    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
  }

 private:
  bool CheckRatio() {
    // To avoid flakes, we also check that gesture scrolling is not in progress
    // which means for a terminal value of the shown ratio (that we're waiting
    // for) sliding is also not in progress and we reached a steady state.
    if (!controller_->IsTopControlsGestureScrollInProgress() &&
        cc::MathUtil::IsWithinEpsilon(controller_->GetShownRatio(),
                                      waiting_for_shown_ratio_)) {
      if (run_loop_)
        run_loop_->Quit();

      return true;
    }

    return false;
  }

  raw_ptr<TestController> controller_;

  std::unique_ptr<base::RunLoop> run_loop_;

  float waiting_for_shown_ratio_ = 0;
};

// Waits for a given |is_gesture_scrolling_in_progress_| value.
class GestureScrollInProgressChangeWaiter : public TestControllerObserver {
 public:
  explicit GestureScrollInProgressChangeWaiter(TestController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }

  ~GestureScrollInProgressChangeWaiter() override {
    controller_->RemoveObserver(this);
  }

  GestureScrollInProgressChangeWaiter(
      const GestureScrollInProgressChangeWaiter&) = delete;
  GestureScrollInProgressChangeWaiter& operator=(
      const GestureScrollInProgressChangeWaiter&) = delete;

  // TestControllerObserver:
  void OnShownRatioChanged(float shown_ratio) override {}

  void OnGestureScrollInProgressChanged(bool in_progress) override {
    if (in_progress == waited_for_in_progress_state_ && run_loop_)
      std::move(run_loop_)->Quit();
  }

  void WaitForInProgressState(bool in_progress_state) {
    if (controller_->IsTopControlsGestureScrollInProgress() ==
        in_progress_state)
      return;

    waited_for_in_progress_state_ = in_progress_state;
    // Use kNestableTasksAllowed to make it possible to wait inside a posted
    // task.
    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
  }

 private:
  raw_ptr<TestController> controller_;

  std::unique_ptr<base::RunLoop> run_loop_;

  bool waited_for_in_progress_state_ = false;
};

}  // namespace

class TopControlsSlideControllerTest : public InProcessBrowserTest {
 public:
  TopControlsSlideControllerTest() = default;

  TopControlsSlideControllerTest(const TopControlsSlideControllerTest&) =
      delete;
  TopControlsSlideControllerTest& operator=(
      const TopControlsSlideControllerTest&) = delete;

  ~TopControlsSlideControllerTest() override = default;

  BrowserView* browser_view() const {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TestController* top_controls_slide_controller() const {
    DCHECK(test_controller_);
    return test_controller_;
  }

  // InProcessBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    // Mark the device is capable of entering tablet mode.
    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);

    // Use first display as internal display. Otherwise, tablet mode is ended
    // on display change.
    command_line->AppendSwitch(switches::kUseFirstDisplayAsInternal);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/top_controls_scroll"));
    ASSERT_TRUE(embedded_test_server()->Start());

    InjectTestController();
  }

  void InjectTestController() {
    browser_view()->top_controls_slide_controller_ = CreateTestController(
        std::move(browser_view()->top_controls_slide_controller_));
  }

  void OpenUrlAtIndex(const GURL& url, int index) {
    ASSERT_TRUE(AddTabAtIndex(index, url, ui::PAGE_TRANSITION_TYPED));
    auto* active_contents = browser_view()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(active_contents));
    SynchronizeBrowserWithRenderer(active_contents);
  }

  void NavigateActiveTabToUrl(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    auto* active_contents = browser_view()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(active_contents));
    SynchronizeBrowserWithRenderer(active_contents);
  }

  void ToggleTabletMode() {
    TopControlsShownRatioWaiter waiter{test_controller_};
    ash::ShellTestApi().SetTabletModeEnabledForTest(!GetTabletModeEnabled());
    waiter.WaitForRatio(1.f);
  }

  bool GetTabletModeEnabled() const {
    return display::Screen::GetScreen()->InTabletMode();
  }

  void CheckBrowserLayout(BrowserView* browser_view,
                          TopChromeShownState shown_state) const {
    const int top_controls_height = browser_view->GetTopControlsHeight();
    EXPECT_NE(top_controls_height, 0);

    ui::Layer* root_view_layer = browser_view->frame()->GetRootView()->layer();

    // The fully-shown and fully-hidden states are terminal states. We check
    // when we reach the steady state. The root view should not have a layer
    // now.
    ASSERT_FALSE(root_view_layer);

    // The contents layer transform should be restored to identity.
    const gfx::Transform expected_transform;
    EXPECT_FALSE(browser_view->GetNativeViewHostsForTopControlsSlide().empty());
    for (auto* host : browser_view->GetNativeViewHostsForTopControlsSlide()) {
      ASSERT_TRUE(host->GetNativeViewContainer());
      ASSERT_TRUE(host->GetNativeViewContainer()->layer());
      CompareTranslations(expected_transform,
                          host->GetNativeViewContainer()->layer()->transform());
    }

    // The BrowserView layout should be adjusted properly:
    const gfx::Rect& top_container_bounds =
        browser_view->top_container()->bounds();
    EXPECT_EQ(top_container_bounds.height(), top_controls_height);

    const int top_container_bottom = top_container_bounds.bottom();
    const gfx::Rect& contents_container_bounds =
        browser_view->contents_container()->bounds();
    EXPECT_EQ(top_container_bottom, contents_container_bounds.y());

    if (shown_state == TopChromeShownState::kFullyHidden) {
      // Top container is shifted up.
      EXPECT_EQ(top_container_bounds.y(), -top_controls_height);

      // Contents should occupy the entire height of the browser view.
      EXPECT_EQ(browser_view->height(),
                browser_view->contents_container()->height());

      // Widget should not allow things to show outside its bounds.
      EXPECT_TRUE(browser_view->frame()->GetLayer()->GetMasksToBounds());

      // The browser controls doesn't shrink the blink viewport size.
      EXPECT_FALSE(browser_view->DoBrowserControlsShrinkRendererSize(
          browser_view->GetActiveWebContents()));
    } else {
      // Top container start at the top.
      EXPECT_EQ(top_container_bounds.y(), 0);

      // Contents should occupy the remainder of the browser view after the top
      // container.
      EXPECT_EQ(browser_view->height() - top_controls_height,
                browser_view->contents_container()->height());

      EXPECT_FALSE(browser_view->frame()->GetLayer()->GetMasksToBounds());

      // The browser controls does shrink the blink viewport size.
      EXPECT_TRUE(browser_view->DoBrowserControlsShrinkRendererSize(
          browser_view->GetActiveWebContents()));
    }
  }

  // Verifies the state of the browser window when the active page is being
  // scrolled by touch gestures in such a way that will result in the top
  // controls shown ratio becoming a fractional value (i.e. sliding top-chrome
  // is in progress).
  // The |layout_test_view| will be used to verify that the BrowserView doesn't
  // get laid out more than once while sliding is in progress.
  // The |expected_shrink_renderer_size| will be checked against the
  // `DoBrowserControlsShrinkRendererSize` bit while sliding.
  void CheckIntermediateScrollStep(LayoutTestView* layout_test_view,
                                   bool expected_shrink_renderer_size) {
    const float shown_ratio = top_controls_slide_controller()->GetShownRatio();

    // This should only be used to verify the state of the browser while sliding
    // is in progress.
    ASSERT_TRUE(shown_ratio != 1.f && shown_ratio != 0.f);

    // While scrolling is in progress, only a single layout is expected.
    EXPECT_TRUE(
        top_controls_slide_controller()->IsTopControlsSlidingInProgress());
    EXPECT_EQ(layout_test_view->layout_count(), 1);

    const int top_controls_height = browser_view()->GetTopControlsHeight();
    EXPECT_NE(top_controls_height, 0);

    ui::Layer* root_view_layer =
        browser_view()->frame()->GetRootView()->layer();

    // While sliding is in progress, the root view paints to a layer.
    ASSERT_TRUE(root_view_layer);

    // This will be called repeatedly while scrolling is in progress. The
    // `DoBrowserControlsShrinkRendererSize` bit should remain the same as the
    // expected value.
    EXPECT_EQ(expected_shrink_renderer_size,
              browser_view()->DoBrowserControlsShrinkRendererSize(
                  browser_view()->GetActiveWebContents()));

    // Check intermediate transforms.
    gfx::Transform expected_transform;
    const float y_translation = top_controls_height * (shown_ratio - 1.f);
    expected_transform.Translate(0, y_translation);

    EXPECT_FALSE(
        browser_view()->GetNativeViewHostsForTopControlsSlide().empty());
    for (auto* host : browser_view()->GetNativeViewHostsForTopControlsSlide()) {
      ASSERT_TRUE(host->GetNativeViewContainer());
      ASSERT_TRUE(host->GetNativeViewContainer()->layer());
      CompareTranslations(expected_transform,
                          host->GetNativeViewContainer()->layer()->transform());
    }
    CompareTranslations(expected_transform, root_view_layer->transform());
  }

  // Using the given |generator| and the start and end points, it generates a
  // gesture scroll sequence with appropriate velocity so that fling gesture
  // scrolls are generated.
  void GenerateGestureFlingScrollSequence(ui::test::EventGenerator* generator,
                                          const gfx::Point& start_point,
                                          const gfx::Point& end_point) {
    DCHECK(generator);
    content::WebContents* contents = browser_view()->GetActiveWebContents();
    generator->GestureScrollSequenceWithCallback(
        start_point, end_point,
        generator->CalculateScrollDurationForFlingVelocity(
            start_point, end_point, 100 /* velocity */, 2 /* steps */),
        2 /* steps */,
        base::BindRepeating(
            [](content::WebContents* contents, ui::EventType,
               const gfx::Vector2dF&) {
              // Give the event a chance to propagate to renderer before sending
              // the next one.
              SynchronizeBrowserWithRenderer(contents);
            },
            contents));
  }

  // Generates a gesture fling scroll sequence to scroll the current page in the
  // given |direction|, and waits for and verifies that top-chrome reaches the
  // given |target_state|.
  void ScrollAndExpectTopChromeToBe(ScrollDirection direction,
                                    TopChromeShownState target_state) {
    aura::Window* browser_window = browser()->window()->GetNativeWindow();
    ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                             browser_window);
    const gfx::Point start_point =
        browser_window->GetBoundsInScreen().CenterPoint();
    const gfx::Point end_point =
        start_point +
        gfx::Vector2d(0, direction == ScrollDirection::kDown ? -100 : 100);

    const float target_ratio =
        target_state == TopChromeShownState::kFullyHidden ? 0.f : 1.f;
    TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
    GenerateGestureFlingScrollSequence(&event_generator, start_point,
                                       end_point);

    waiter.WaitForRatio(target_ratio);
    EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(),
                    target_ratio);
    CheckBrowserLayout(browser_view(), target_state);
  }

 private:
  std::unique_ptr<TopControlsSlideController> CreateTestController(
      std::unique_ptr<TopControlsSlideController> real_controller) {
    DCHECK(real_controller);
    auto controller =
        std::make_unique<TestController>(std::move(real_controller));
    test_controller_ = controller.get();
    return std::move(controller);
  }

  raw_ptr<TestController, DanglingUntriaged> test_controller_ =
      nullptr;  // Not owned.
};

namespace {

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, DisabledForHostedApps) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::mojom::WindowShowState::kDefault;
  Browser* browser = Browser::Create(params);
  AddBlankTabAndShow(browser);

  ASSERT_TRUE(browser->is_type_app());

  // No slide controller gets created for hosted apps.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  EXPECT_FALSE(browser_view->top_controls_slide_controller());

  // Renderer will get a zero-height top controls.
  EXPECT_EQ(browser_view->GetTopControlsHeight(), 0);
  EXPECT_FALSE(browser_view->DoBrowserControlsShrinkRendererSize(
      browser_view->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       EnabledOnlyForTabletNonImmersiveModes) {
  EXPECT_FALSE(GetTabletModeEnabled());
  AddBlankTabAndShow(browser());
  // For a normal browser, the controller is created.
  ASSERT_TRUE(top_controls_slide_controller());
  // But the behavior is disabled since we didn't go to tablet mode yet.
  EXPECT_FALSE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  // The browser reports a zero height for top-chrome UIs when the behavior is
  // disabled, so the render doesn't think it needs to move the top controls.
  EXPECT_EQ(browser_view()->GetTopControlsHeight(), 0);
  EXPECT_FALSE(browser_view()->DoBrowserControlsShrinkRendererSize(
      browser_view()->GetActiveWebContents()));

  // Now enable tablet mode.
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());

  // The behavior is enabled, but the shown ratio remains at 1.f since no page
  // scrolls happened yet.
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  EXPECT_NE(browser_view()->GetTopControlsHeight(), 0);
  EXPECT_TRUE(browser_view()->DoBrowserControlsShrinkRendererSize(
      browser_view()->GetActiveWebContents()));

  // Immersive fullscreen mode disables the behavior.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view()->IsFullscreen());
  EXPECT_FALSE(top_controls_slide_controller()->IsEnabled());
  EXPECT_EQ(browser_view()->GetTopControlsHeight(), 0);
  EXPECT_FALSE(browser_view()->DoBrowserControlsShrinkRendererSize(
      browser_view()->GetActiveWebContents()));

  // Exit immersive mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_view()->IsFullscreen());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_NE(browser_view()->GetTopControlsHeight(), 0);
  EXPECT_TRUE(browser_view()->DoBrowserControlsShrinkRendererSize(
      browser_view()->GetActiveWebContents()));

  ToggleTabletMode();
  EXPECT_FALSE(GetTabletModeEnabled());
  EXPECT_FALSE(top_controls_slide_controller()->IsEnabled());
  EXPECT_EQ(browser_view()->GetTopControlsHeight(), 0);
  EXPECT_FALSE(browser_view()->DoBrowserControlsShrinkRendererSize(
      browser_view()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, TestScrollingPage) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Navigate to our test page that has a long vertical content which we can use
  // to test page scrolling.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);

  // It's possible to hide top chrome with gesture scrolling.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);

  // Perform another gesture scroll in the opposite direction and expect top-
  // chrome to be fully shown.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kUp,
                               TopChromeShownState::kFullyShown);
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, TestCtrlL) {
  // Switch to tablet mode, and scroll until the top controls are fully hidden.
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  auto* active_contents = browser_view()->GetActiveWebContents();
  SCOPED_TRACE("Scrolling to fully hide the top controls.");
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);
  SynchronizeBrowserWithRenderer(active_contents);

  // Hit Ctrl+L which should focus the omnibox. This should unhide the top
  // controls.
  SCOPED_TRACE("Firing Ctrl+L.");
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  event_generator.PressAndReleaseKeyAndModifierKeys(ui::VKEY_L,
                                                    ui::EF_CONTROL_DOWN);
  waiter.WaitForRatio(1.f);
  EXPECT_TRUE(browser_view()->GetLocationBarView()->omnibox_view()->HasFocus());
}

// Fails on Linux ChromiumOS MSan Tests (https://crbug.com/1194575).
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestScrollingPageAndSwitchingToNTP \
  DISABLED_TestScrollingPageAndSwitchingToNTP
#else
#define MAYBE_TestScrollingPageAndSwitchingToNTP \
  TestScrollingPageAndSwitchingToNTP
#endif
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       MAYBE_TestScrollingPageAndSwitchingToNTP) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Add a tab containing a local NTP page. NTP pages are not permitted to hide
  // top-chrome with scrolling.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  // Navigate to our test page that has a long vertical content which we can use
  // to test page scrolling.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  // Scroll the `top_controls_scroll.html` page such that top-chrome is now
  // fully hidden.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);

  // Simulate (Ctrl + Tab) shortcut to select the next tab. Top-chrome should
  // show automatically.
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Since this is the NTP page, gesture scrolling down will not hide
  // top-chrome. It will remain fully shown.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyShown);

  // Switch back to the scrollable page, it should be possible now to hide top-
  // chrome.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);

  // The `DoBrowserControlsShrinkRendererSize` bit is separately tracked for
  // each tab.
  auto* tab_strip_model = browser()->tab_strip_model();
  auto* scrollable_page_contents = tab_strip_model->GetWebContentsAt(0);
  auto* ntp_contents = tab_strip_model->GetWebContentsAt(1);
  EXPECT_TRUE(
      browser_view()->DoBrowserControlsShrinkRendererSize(ntp_contents));
  EXPECT_FALSE(browser_view()->DoBrowserControlsShrinkRendererSize(
      scrollable_page_contents));
}

// Fails on Linux Chromium OS Tests (https://crbug.com/1191327).
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestClosingATab DISABLED_TestClosingATab
#else
#define MAYBE_TestClosingATab TestClosingATab
#endif
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, MAYBE_TestClosingATab) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Navigate to our test scrollable page.
  NavigateActiveTabToUrl(
      embedded_test_server()->GetURL("/top_controls_scroll.html"));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  // Scroll to fully hide top-chrome.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);

  // Simulate (Ctrl + T) by inserting a new tab. Expect top-chrome to be fully
  // shown.
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  chrome::NewTab(browser());
  waiter.WaitForRatio(1.f);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Now simulate (Ctrl + W) by closing this newly created tab, expect to return
  // to the scrollable page tab, which used to have a top-chrome shown ratio of
  // 0. We should expect that it will animate back to a shown ratio of 1.
  // This test is needed to make sure that the native view of the web contents
  // of the newly selected tab after the current one is closed will attach to
  // the browser's native view host *before* we attempt to make any changes to
  // its top-chrome shown ratio.
  chrome::CloseTab(browser());
  waiter.WaitForRatio(1.f);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // It is still possible to slide top-chrome up.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);
}

// Sheriff 2022/02/25; flaky test crbug/1300462
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       DISABLED_TestFocusEditableElements) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Navigate to our test page that has a long vertical content which we can use
  // to test page scrolling.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);

  SCOPED_TRACE("Initial scroll to hide the top controls.");
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);

  // Define an internal lambda that returns the javascript function body that
  // can be executed on the focus on `top_controls_scroll.html` page to focus on
  // an editable text input field in the page, or blur its focus depending on
  // the |should_focus| parameter.
  auto get_js_function_body = [](bool should_focus) -> std::string {
    constexpr size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    base::strings::SafeSPrintf(
        buffer,
        "((function() {"
        "    var editableElement ="
        "        document.getElementById('editable-element');"
        "    if (editableElement) {"
        "      editableElement.%s();"
        "      return true;"
        "    }"
        "    return false;"
        "  })());",
        should_focus ? "focus" : "blur");
    return std::string(buffer);
  };

  // Focus on the editable element in the page and expect that top-chrome will
  // be shown.
  SCOPED_TRACE("Focus an editable element in the page.");
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(
                      contents, get_js_function_body(true /* should_focus */)));
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Now try scrolling in a way that would normally hide top-chrome, and expect
  // that top-chrome will be forced shown as long as the editable element is
  // focused.
  SCOPED_TRACE("Attempting a scroll which will not hide the top controls.");
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyShown);

  // Now blur the focused editable element. Expect that top-chrome can now be
  // hidden with gesture scrolls.
  SCOPED_TRACE("Bluring the focus away from the editable element.");
  EXPECT_EQ(true, content::EvalJs(contents, get_js_function_body(
                                                false /* should_focus */)));
  // Evaluate an empty sentence to make sure that the event processing is done
  // in the content.
  std::ignore = content::EvalJs(contents, ";");

  SCOPED_TRACE("Scroll to hide should now work.");
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);
}

// Used to wait for the browser view to change its bounds as a result of display
// rotation.
class BrowserViewLayoutWaiter : public views::ViewObserver {
 public:
  explicit BrowserViewLayoutWaiter(BrowserView* browser_view)
      : browser_view_(browser_view) {
    browser_view->AddObserver(this);
  }

  BrowserViewLayoutWaiter(const BrowserViewLayoutWaiter&) = delete;
  BrowserViewLayoutWaiter& operator=(const BrowserViewLayoutWaiter&) = delete;

  ~BrowserViewLayoutWaiter() override { browser_view_->RemoveObserver(this); }

  void Wait() {
    if (view_bounds_changed_) {
      view_bounds_changed_ = false;
      return;
    }

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    view_bounds_changed_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  raw_ptr<BrowserView> browser_view_;

  bool view_bounds_changed_ = false;

  std::unique_ptr<base::RunLoop> run_loop_;
};

// TODO(crbug.com/40224646): Flaky under dbg and sanitizers.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_DisplayRotation DISABLED_DisplayRotation
#else
#define MAYBE_DisplayRotation DisplayRotation
#endif
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, MAYBE_DisplayRotation) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Maximizing the browser window makes the browser view layout more
  // predictable with display rotation, as it's just resized to match the
  // display bounds.
  browser_view()->frame()->Maximize();

  // Navigate to our scrollable test page, scroll with touch gestures so that
  // top-chrome is fully hidden.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  gfx::Point start_point = event_generator.current_screen_location();
  gfx::Point end_point = start_point + gfx::Vector2d(0, -100);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

  // Try all possible rotations. Changing display rotation should *not* unhide
  // top chrome.
  const std::vector<crosapi::mojom::DisplayRotationOptions> rotations_to_try = {
      crosapi::mojom::DisplayRotationOptions::k90Degrees,
      crosapi::mojom::DisplayRotationOptions::k180Degrees,
      crosapi::mojom::DisplayRotationOptions::k270Degrees,
      crosapi::mojom::DisplayRotationOptions::kZeroDegrees,
  };

  mojo::Remote<crosapi::mojom::CrosDisplayConfigController> cros_display_config;
  ash::BindCrosDisplayConfigController(
      cros_display_config.BindNewPipeAndPassReceiver());

  base::test::TestFuture<std::vector<crosapi::mojom::DisplayUnitInfoPtr>>
      info_list_future;
  cros_display_config->GetDisplayUnitInfoList(false /* single_unified */,
                                              info_list_future.GetCallback());
  auto info_list = info_list_future.Take();
  for (const crosapi::mojom::DisplayUnitInfoPtr& display_unit_info :
       info_list) {
    const std::string display_id = display_unit_info->id;
    for (const auto& rotation : rotations_to_try) {
      BrowserViewLayoutWaiter browser_view_layout_waiter(browser_view());
      auto config_properties = crosapi::mojom::DisplayConfigProperties::New();
      config_properties->rotation =
          crosapi::mojom::DisplayRotation::New(rotation);
      base::test::TestFuture<crosapi::mojom::DisplayConfigResult> result_future;
      cros_display_config->SetDisplayProperties(
          display_id, std::move(config_properties),
          crosapi::mojom::DisplayConfigSource::kUser,
          result_future.GetCallback());
      EXPECT_EQ(result_future.Take(),
                crosapi::mojom::DisplayConfigResult::kSuccess);

      // Wait for the browser view to change its bounds as a result of display
      // rotation.
      browser_view_layout_waiter.Wait();

      // Make sure top-chrome is still hidden.
      waiter.WaitForRatio(0.f);
      EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
      CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

      // Now perform gesture scrolls to show top-chrome, make sure everything
      // looks good in this rotation.
      // Update the start and end points after rotation.
      start_point = browser_window->GetBoundsInRootWindow().CenterPoint();
      end_point = start_point + gfx::Vector2d(0, -100);

      // Make sure to send them in screen coordinates to make sure rotation
      // is taken into consideration.
      browser_window->GetRootWindow()->GetHost()->ConvertDIPToScreenInPixels(
          &start_point);
      browser_window->GetRootWindow()->GetHost()->ConvertDIPToScreenInPixels(
          &end_point);
      GenerateGestureFlingScrollSequence(&event_generator, end_point,
                                         start_point);
      waiter.WaitForRatio(1.f);
      EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
      CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

      // Scroll again to hide top-chrome in preparation for the next rotation
      // iteration.
      GenerateGestureFlingScrollSequence(&event_generator, start_point,
                                         end_point);
      waiter.WaitForRatio(0.f);
      EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
      CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);
    }
  }
}

// Waits for receiving an IPC message from the render frame that the page state
// has been updated. This makes sure that the renderer now sees the new top
// controls height if it changed.
class PageStateUpdateWaiter : content::WebContentsObserver {
 public:
  explicit PageStateUpdateWaiter(content::WebContents* contents)
      : WebContentsObserver(contents) {}

  PageStateUpdateWaiter(const PageStateUpdateWaiter&) = delete;
  PageStateUpdateWaiter& operator=(const PageStateUpdateWaiter&) = delete;

  ~PageStateUpdateWaiter() override = default;

  void Wait() {
    run_loop_.Run();
    SynchronizeBrowserWithRenderer(web_contents());
  }

  // content::WebContentsObserver:
  void NavigationEntryChanged(
      const content::EntryChangedDetails& change_details) override {
    // This notification is triggered upon receiving the
    // |FrameHostMsg_UpdateState| message in the |RenderFrameHostImpl|, which
    // indicates that the page state now has been updated, and we can now
    // proceeed with testing gesture scrolls behavior.
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

// Verifies that we ignore the shown ratios sent from widgets other than that of
// the main frame (such as widgets of the drop-down menus in web pages).
// https://crbug.com/891471.
// TODO(crbug.com/40848345): Flaky for dbg and ASan builds.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)
#define MAYBE_TestDropDowns DISABLED_TestDropDowns
#else
#define MAYBE_TestDropDowns TestDropDowns
#endif
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, MAYBE_TestDropDowns) {
  browser_view()->frame()->Maximize();
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);

  // Send a mouse click event that should open the popup drop-down menu of the
  // <select> html element on the page.
  // Note that if a non-main-frame widget is created, its LayerTreeHostImpl's
  // `top_controls_shown_ratio_` (which is initialized to 0.f) will be sent to
  // the browser when a new compositor frame gets generated. If this shown ratio
  // value is not ignored, top-chrome will immediately hide, which will result
  // in a BrowserView layout and the immediate closure of the drop-down menu. We
  // verify below that this doesn't happen, the menu remains open, and it's
  // possible to select another option in the drop-down menu.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PageStateUpdateWaiter page_state_update_waiter(contents);
  page_state_update_waiter.Wait();

  // Programmatically focus the <select> element, and verify that the element
  // has been focused.
  EXPECT_EQ(true, content::EvalJs(contents, "focusSelectElement();"));
  EXPECT_EQ(true, content::EvalJs(contents, "selectFocused;"));

  // Hit <enter> on the keyboard, then <down> three times, then <enter> again to
  // select the fourth option.
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow());
  auto send_key_event = [&event_generator](ui::KeyboardCode keycode) {
    event_generator.PressKey(keycode, ui::EF_NONE);
    event_generator.ReleaseKey(keycode, ui::EF_NONE);
  };
  send_key_event(ui::VKEY_RETURN);
  send_key_event(ui::VKEY_DOWN);
  send_key_event(ui::VKEY_DOWN);
  send_key_event(ui::VKEY_DOWN);
  send_key_event(ui::VKEY_RETURN);
  // Evaluate an empty sentence to make sure that the event processing is done
  // in the content.
  std::ignore = content::EvalJs(contents, ";");

  // Verify that the selected option has changed and the fourth option is
  // selected.
  EXPECT_EQ(true, content::EvalJs(contents, "selectChanged;"));
  EXPECT_EQ("4", content::EvalJs(contents, "getSelectedValue();"));
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       TestScrollingMaximizedPageBeforeGoingToTabletMode) {
  // If the page exists in a maximized browser window before going to tablet
  // mode, the layout that results from going to tablet mode does not change
  // the size of the page viewport. Hence, the visual properties of the renderer
  // and the browser are not automatically synchrnoized. But going to tablet
  // mode enables the top-chrome sliding feature (i.e.
  // BrowserView::GetTopControlsHeight() now returns a non-zero value). We must
  // make sure that we synchronize the visual properties manually, otherwise
  // the renderer will never get the new top-controls height.
  browser_view()->frame()->Maximize();

  // Navigate to our test scrollable page.
  NavigateActiveTabToUrl(
      embedded_test_server()->GetURL("/top_controls_scroll.html"));
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  EXPECT_EQ(browser_view()->GetTopControlsHeight(), 0);

  // Switch to tablet mode. This should trigger a synchronize visual properties
  // event with the renderer so that it can get the correct top controls height
  // now that the top-chrome slide feature is enabled. Otherwise hiding top
  // chrome with gesture scrolls won't be possible at all.
  PageStateUpdateWaiter page_state_update_waiter(active_contents);
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  EXPECT_NE(browser_view()->GetTopControlsHeight(), 0);
  page_state_update_waiter.Wait();

  // Scroll to fully hide top-chrome.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);
}

// Waits for a fractional value of the top controls shown ratio, upon which it
// will invoke the |on_intermediate_ratio_callback| which can be used to verify
// the state of the browser while sliding is in progress.
class IntermediateShownRatioWaiter : public TestControllerObserver {
 public:
  explicit IntermediateShownRatioWaiter(
      TestController* controller,
      base::OnceClosure on_intermediate_ratio_callback)
      : controller_(controller),
        on_intermediate_ratio_callback_(
            std::move(on_intermediate_ratio_callback)) {
    controller_->AddObserver(this);
  }

  IntermediateShownRatioWaiter(const IntermediateShownRatioWaiter&) = delete;
  IntermediateShownRatioWaiter& operator=(const IntermediateShownRatioWaiter&) =
      delete;

  ~IntermediateShownRatioWaiter() override {
    controller_->RemoveObserver(this);
  }

  bool seen_intermediate_ratios() const { return seen_intermediate_ratios_; }

  // TestControllerObserver:
  void OnShownRatioChanged(float shown_ratio) override {
    seen_intermediate_ratios_ |= shown_ratio > 0.0 && shown_ratio < 1.f;
    if (!seen_intermediate_ratios_)
      return;

    if (on_intermediate_ratio_callback_)
      std::move(on_intermediate_ratio_callback_).Run();

    if (run_loop_)
      run_loop_->Quit();
  }

  void OnGestureScrollInProgressChanged(bool in_progress) override {}

  void Wait() {
    if (seen_intermediate_ratios_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
  }

 private:
  raw_ptr<TestController> controller_;

  std::unique_ptr<base::RunLoop> run_loop_;

  base::OnceClosure on_intermediate_ratio_callback_;

  bool seen_intermediate_ratios_ = false;
};

// TODO(crbug.com/40676580): Test is flaky.
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       DISABLED_TestIntermediateSliding) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Navigate to our test page that has a long vertical content which we can use
  // to test page scrolling.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  content::WebContents* active_contents =
      browser_view()->GetActiveWebContents();
  PageStateUpdateWaiter page_state_update_waiter(active_contents);
  page_state_update_waiter.Wait();
  EXPECT_TRUE(
      browser_view()->DoBrowserControlsShrinkRendererSize(active_contents));

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  const gfx::Point start_point = event_generator.current_screen_location();
  const gfx::Point end_point = start_point + gfx::Vector2d(0, -100);

  LayoutTestView layout_test_view{browser_view()};
  {
    // We will start scrolling while top-chrome is fully shown, in which case
    // the `DoBrowserControlsShrinkRendererSize` bit is true ...
    EXPECT_TRUE(
        browser_view()->DoBrowserControlsShrinkRendererSize(active_contents));
    // ... It should change to false at the beginning of sliding and remain
    // false while sliding is in progress.
    const bool expected_shrink_renderer_size = false;

    TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
    IntermediateShownRatioWaiter fractional_ratio_waiter(
        top_controls_slide_controller(),
        base::BindOnce(
            &TopControlsSlideControllerTest::CheckIntermediateScrollStep,
            base::Unretained(this), &layout_test_view,
            expected_shrink_renderer_size));
    layout_test_view.Reset();

    event_generator.PressTouch();
    SynchronizeBrowserWithRenderer(active_contents);
    for (gfx::Point current_point = start_point;
         !(current_point == end_point ||
           fractional_ratio_waiter.seen_intermediate_ratios());
         current_point += gfx::Vector2d(0, -1)) {
      event_generator.MoveTouch(current_point);
      SynchronizeBrowserWithRenderer(active_contents);
    }
    event_generator.MoveTouch(end_point);
    event_generator.ReleaseTouch();
    waiter.WaitForRatio(0.f);

    EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
    CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

    // Now that sliding ended, and top-chrome is fully hidden, the
    // `DoBrowserControlsShrinkRendererSize` bit should remain false ...
    EXPECT_FALSE(
        browser_view()->DoBrowserControlsShrinkRendererSize(active_contents));
  }

  SynchronizeBrowserWithRenderer(active_contents);

  {
    // ... and when scrolling in the other direction towards a fully shown
    // top-chrome, it should remain false while sliding is in progress.
    const bool expected_shrink_renderer_size = false;

    TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
    IntermediateShownRatioWaiter fractional_ratio_waiter(
        top_controls_slide_controller(),
        base::BindOnce(
            &TopControlsSlideControllerTest::CheckIntermediateScrollStep,
            base::Unretained(this), &layout_test_view,
            expected_shrink_renderer_size));
    layout_test_view.Reset();
    event_generator.PressTouch();
    SynchronizeBrowserWithRenderer(active_contents);
    for (gfx::Point current_point = end_point;
         !(current_point == start_point ||
           fractional_ratio_waiter.seen_intermediate_ratios());
         current_point += gfx::Vector2d(0, 1)) {
      event_generator.MoveTouch(current_point);
      SynchronizeBrowserWithRenderer(active_contents);
    }
    event_generator.MoveTouch(start_point);
    event_generator.ReleaseTouch();
    waiter.WaitForRatio(1.f);

    EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
    CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

    // Now that sliding ended, and top-chrome is fully shown, the
    // `DoBrowserControlsShrinkRendererSize` bit should be true.
    EXPECT_TRUE(
        browser_view()->DoBrowserControlsShrinkRendererSize(active_contents));
  }
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       DisplayMetricsChangeWhileInProgress) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Triggers a display metrics change event while both gesture scrolling and
  // sliding are in progress.
  auto rotate_display_while_in_progress =
      [](content::WebContents* contents, TestController* slide_controller,
         ui::test::EventGenerator* generator) {
        ASSERT_TRUE(slide_controller->IsTopControlsGestureScrollInProgress());
        ASSERT_TRUE(slide_controller->IsTopControlsSlidingInProgress());

        // Trigger the keyboard shrotcut for changing the device scale factor.
        // This should result in a display metric change.
        constexpr int kFlags = ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;
        generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_OEM_PLUS, kFlags);

        // Test that as result of the above, sliding has been temporarily
        // disabled, and that the top controls are fully shown.
        EXPECT_FALSE(slide_controller->IsEnabled());
        EXPECT_FLOAT_EQ(slide_controller->GetShownRatio(), 1.f);
        // Even though gesture scrolling hasn't ended.
        EXPECT_TRUE(slide_controller->IsTopControlsGestureScrollInProgress());
      };

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  const gfx::Point start_point = event_generator.current_screen_location();
  const gfx::Point end_point = start_point + gfx::Vector2d(0, -100);

  auto* active_contents = browser_view()->GetActiveWebContents();
  IntermediateShownRatioWaiter fractional_ratio_waiter(
      top_controls_slide_controller(),
      base::BindOnce(rotate_display_while_in_progress, active_contents,
                     top_controls_slide_controller(), &event_generator));

  event_generator.set_current_screen_location(start_point);
  event_generator.PressTouch();
  SynchronizeBrowserWithRenderer(active_contents);
  for (gfx::Point current_point = start_point;
       !(current_point == end_point ||
         fractional_ratio_waiter.seen_intermediate_ratios());
       current_point += gfx::Vector2d(0, -1)) {
    event_generator.MoveTouch(current_point);
    SynchronizeBrowserWithRenderer(active_contents);
  }

  // Release touch and wait for gesture scrolling to end.
  GestureScrollInProgressChangeWaiter waiter{top_controls_slide_controller()};
  event_generator.ReleaseTouch();
  waiter.WaitForInProgressState(false);

  // Expect that sliding has been re-enabled, and the top controls are still
  // fully shown.
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);
}

// Sheriff 2022/04/18; flaky test crbug/1317068
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       DISABLED_TestPermissionBubble) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  const GURL url(embedded_test_server()->GetURL("/top_controls_scroll.html"));
  OpenUrlAtIndex(url, 0);
  content::WebContents* active_contents =
      browser_view()->GetActiveWebContents();
  PageStateUpdateWaiter page_state_update_waiter(active_contents);
  page_state_update_waiter.Wait();
  EXPECT_TRUE(
      browser_view()->DoBrowserControlsShrinkRendererSize(active_contents));

  // Hide top chrome.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);

  // Fire a geolocation permission request, which should show a permission
  // request bubble resulting in top chrome unhiding.
  auto decided = [](ContentSetting, bool, bool) {};
  permissions::PermissionRequest permission_request(
      url, permissions::RequestType::kGeolocation, true /* user_gesture */,
      base::BindRepeating(decided), base::DoNothing() /* delete_callback */);
  auto* permission_manager =
      permissions::PermissionRequestManager::FromWebContents(active_contents);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  permission_manager->AddRequest(active_contents->GetPrimaryMainFrame(),
                                 &permission_request);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);
  EXPECT_TRUE(permission_manager->IsRequestInProgress());

  // It shouldn't be possible to hide top-chrome as long as the bubble is
  // visible.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyShown);

  // Dismiss the bubble.
  permission_manager->Dismiss();
  EXPECT_FALSE(permission_manager->IsRequestInProgress());
  SynchronizeBrowserWithRenderer(active_contents);

  // Now it is possible to hide top-chrome again.
  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);
}

// Flaky on ChromeOS bots. https://crbug.com/1033648
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       DISABLED_TestToggleChromeVox) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  content::WebContents* active_contents =
      browser_view()->GetActiveWebContents();
  PageStateUpdateWaiter page_state_update_waiter(active_contents);
  page_state_update_waiter.Wait();
  EXPECT_TRUE(
      browser_view()->DoBrowserControlsShrinkRendererSize(active_contents));

  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);

  // Enable Chromevox (spoken feedback) and expect that top-chrome will be fully
  // shown, and sliding top-chrome is no longer enabled.
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  SynchronizeBrowserWithRenderer(active_contents);
  EXPECT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  EXPECT_TRUE(
      browser_view()->DoBrowserControlsShrinkRendererSize(active_contents));

  // Now disable Chromevox, and expect it's now possible to hide top-chrome with
  // gesture scrolling.
  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  SynchronizeBrowserWithRenderer(active_contents);
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  ScrollAndExpectTopChromeToBe(ScrollDirection::kDown,
                               TopChromeShownState::kFullyHidden);
}

// Regression test for https://crbug.com/1163276.
// TODO(crbug.com/40174370): Test times out flakily.
IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       DISABLED_NoCrashOnNewTabWhileScrolling) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  NavigateActiveTabToUrl(
      embedded_test_server()->GetURL("/top_controls_scroll.html"));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  const gfx::Point start_point = event_generator.current_screen_location();
  auto* active_contents = browser_view()->GetActiveWebContents();

  // Create a new tab while gesture scrolling is in progress, and top-chrome is
  // fully hidden.
  event_generator.PressTouch();
  SynchronizeBrowserWithRenderer(active_contents);
  auto current_point = start_point;
  while (!(
      top_controls_slide_controller()->IsTopControlsGestureScrollInProgress() &&
      top_controls_slide_controller()->GetShownRatio() == 0.f)) {
    current_point += gfx::Vector2d(0, -1);
    event_generator.MoveTouch(current_point);
    SynchronizeBrowserWithRenderer(active_contents);
  }
  constexpr int kFlags = ui::EF_CONTROL_DOWN;
  event_generator.PressAndReleaseKeyAndModifierKeys(ui::VKEY_T, kFlags);
  event_generator.ReleaseTouch();
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
}

// TODO(crbug.com/40638200): Add test coverage that covers using WebUITabStrip.

}  // namespace
