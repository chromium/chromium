// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace {

enum class WindowState { kNormal, kMaximized, kImmersiveMode };

std::string WindowStateToString(WindowState state) {
  constexpr std::array kWindowStateNames = {"Normal", "Maximized",
                                            "ImmersiveMode"};
  return kWindowStateNames[static_cast<int>(state)];
}

class WidgetResizedWaiter : public views::WidgetObserver {
 public:
  explicit WidgetResizedWaiter(views::Widget* widget)
      : original_bounds_(widget->GetWindowBoundsInScreen()) {
    observation_.Observe(widget);
  }

  bool Wait() {
    run_loop_.Run();
    return resized_;
  }

 private:
  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget*,
                             const gfx::Rect& new_bounds) override {
    resized_ =
        new_bounds != original_bounds_ && new_bounds.Contains(original_bounds_);
    run_loop_.Quit();
  }
  void OnWidgetDestroying(views::Widget*) override { observation_.Reset(); }

  const gfx::Rect original_bounds_;
  bool resized_ = false;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

gfx::Rect GetBoundsInWindow(views::View* view) {
  auto* const widget = view->GetWidget();
  auto* const root = widget->GetRootView();
  return views::View::ConvertRectToTarget(view, root, view->GetLocalBounds());
}

}  // namespace

class BrowserViewLayoutDelegateImplBrowsertest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<WindowState> {
 public:
  BrowserViewLayoutDelegateImplBrowsertest() = default;
  ~BrowserViewLayoutDelegateImplBrowsertest() override = default;

  void ApplyWindowState(Browser* browser) {
    switch (GetParam()) {
      case WindowState::kNormal:
        break;
      case WindowState::kMaximized: {
        auto* const widget =
            BrowserView::GetBrowserViewForBrowser(browser)->GetWidget();
        WidgetResizedWaiter waiter(widget);
        widget->Maximize();
        ASSERT_TRUE(waiter.Wait());
        ASSERT_TRUE(widget->IsMaximized());
        break;
      }
      case WindowState::kImmersiveMode: {
        auto* const controller = ImmersiveModeController::From(browser);
        // Note: this will enter immersive mode without going fullscreen.
        controller->SetEnabled(true);
        ASSERT_TRUE(controller->IsEnabled());
        immersive_mode_lock_ = controller->GetRevealedLock(
            ImmersiveModeController::AnimateReveal::ANIMATE_REVEAL_NO);
        ASSERT_TRUE(controller->IsRevealed());
        break;
      }
    }
  }

  void TearDownOnMainThread() override {
    immersive_mode_lock_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  Browser* CreateAppBrowser() {
    const GURL kAppUrl("https://test.com");
    const auto app_id = web_app::test::InstallDummyWebApp(browser()->profile(),
                                                          "App Name", kAppUrl);
    return web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  std::unique_ptr<ImmersiveRevealedLock> immersive_mode_lock_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserViewLayoutDelegateImplBrowsertest,
// Immersive mode is only available on Mac and ChromeOS, but Mac does not
// support maximization in the same sense as other platforms.
#if BUILDFLAG(IS_MAC)
                         testing::Values(WindowState::kNormal,
                                         WindowState::kImmersiveMode),
#elif BUILDFLAG(IS_CHROMEOS)
                         testing::Values(WindowState::kNormal,
                                         WindowState::kMaximized,
                                         WindowState::kImmersiveMode),
#else  // Linux or Windows
                         testing::Values(WindowState::kNormal,
                                         WindowState::kMaximized),
#endif
                         [](testing::TestParamInfo<WindowState> info) {
                           return WindowStateToString(info.param);
                         });

IN_PROC_BROWSER_TEST_P(BrowserViewLayoutDelegateImplBrowsertest,
                       Screenshot_TabbedBrowser) {
  ApplyWindowState(browser());

  gfx::Rect bounds;
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Screenshot not supported on all platforms"),
      WithView(kBrowserViewElementId,
               [&bounds](BrowserView* browser_view) {
                 TabStrip* const tabstrip = browser_view->tabstrip();
                 tabstrip->InvalidateLayout();
                 views::test::RunScheduledLayout(browser_view);
                 bounds = GetBoundsInWindow(tabstrip);
                 bounds.set_x(0);
                 bounds.set_width(browser_view->width());
               }),
      Screenshot(kBrowserViewElementId, "tabstrip_region", "6956029",
                 std::ref(bounds)));
}

IN_PROC_BROWSER_TEST_P(BrowserViewLayoutDelegateImplBrowsertest,
                       Screenshot_AppBrowser) {
  // App browser can't be created inside RunTestSequence due to RunLoop issues.
  auto* const app_browser = CreateAppBrowser();

  ApplyWindowState(app_browser);

  gfx::Rect bounds;
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Screenshot not supported on all platforms"),
      InContext(
          BrowserElements::From(app_browser)->GetContext(),
          WaitForShow(kBrowserViewElementId),
          WithView(kBrowserViewElementId,
                   [&bounds](BrowserView* browser_view) {
                     WebAppFrameToolbarView* const toolbar =
                         browser_view->web_app_frame_toolbar_for_testing();
                     toolbar->InvalidateLayout();
                     views::test::RunScheduledLayout(browser_view);
                     bounds = GetBoundsInWindow(toolbar);
                     bounds.set_x(0);
                     bounds.set_width(browser_view->width());
                   }),
          Screenshot(kBrowserViewElementId, "tabstrip_region", "6956029",
                     std::ref(bounds))));
}
