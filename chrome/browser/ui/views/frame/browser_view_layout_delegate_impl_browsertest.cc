// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout_delegate_impl.h"

#include <memory>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_utils.h"
#include "url/gurl.h"

class BrowserViewLayoutDelegateImplBrowsertest : public InteractiveBrowserTest {
 public:
  BrowserViewLayoutDelegateImplBrowsertest() = default;
  ~BrowserViewLayoutDelegateImplBrowsertest() override = default;

  gfx::Rect GetBoundsInWindow(views::View* view, views::View* window) {
    return views::View::ConvertRectToTarget(view, window,
                                            view->GetLocalBounds());
  }

  void SetUseLayoutDelegate(Browser* browser, bool use_new_delegate) {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    auto* const layout = browser_view->GetBrowserViewLayout();
    if (use_new_delegate) {
      layout->SetDelegateForTesting(
          std::make_unique<BrowserViewLayoutDelegateImplNew>(*browser_view));
    } else {
      layout->SetDelegateForTesting(
          std::make_unique<BrowserViewLayoutDelegateImplOld>(*browser_view));
    }
    views::test::RunScheduledLayout(browser_view);
  }

  Browser* CreateAppBrowser() {
    const GURL kAppUrl("https://test.com");
    const auto app_id = web_app::test::InstallDummyWebApp(browser()->profile(),
                                                          "App Name", kAppUrl);
    return web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// TODO(crbug.com/445725696): Fix failing test on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CompareOldAndNewLayout_TabbedBrowser \
  DISABLED_CompareOldAndNewLayout_TabbedBrowser
#else
#define MAYBE_CompareOldAndNewLayout_TabbedBrowser \
  CompareOldAndNewLayout_TabbedBrowser
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewLayoutDelegateImplBrowsertest,
                       MAYBE_CompareOldAndNewLayout_TabbedBrowser) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  TabStrip* const tabstrip = browser_view->tabstrip();
  ToolbarView* const toolbar = browser_view->toolbar();
  gfx::Rect tabstrip_bounds;
  gfx::Rect toolbar_bounds;

  // Get the bounds using the old layout.
  {
    SetUseLayoutDelegate(browser(), false);
    tabstrip_bounds = GetBoundsInWindow(tabstrip, browser_view);
    toolbar_bounds = GetBoundsInWindow(toolbar, browser_view);
  }

  // Get the bounds in the new layout and confirm that they match.
  {
    SetUseLayoutDelegate(browser(), true);
    EXPECT_EQ(tabstrip_bounds, GetBoundsInWindow(tabstrip, browser_view))
        << "Tabstrip bounds differ.";
    EXPECT_EQ(toolbar_bounds, GetBoundsInWindow(toolbar, browser_view))
        << "Toolbar bounds differ.";
  }
}

// TODO(crbug.com/445725696): Fix failing test on Mac and Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_CompareOldAndNewLayout_AppBrowser \
  DISABLED_CompareOldAndNewLayout_AppBrowser
#else
#define MAYBE_CompareOldAndNewLayout_AppBrowser \
  CompareOldAndNewLayout_AppBrowser
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewLayoutDelegateImplBrowsertest,
                       MAYBE_CompareOldAndNewLayout_AppBrowser) {
  Browser* const app_browser = CreateAppBrowser();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  WebAppFrameToolbarView* const toolbar =
      browser_view->web_app_frame_toolbar_for_testing();
  gfx::Rect toolbar_bounds;

  // Get the bounds using the old layout.
  {
    SetUseLayoutDelegate(app_browser, false);
    toolbar_bounds = GetBoundsInWindow(toolbar, browser_view);
  }

  // Get the bounds in the new layout and confirm that they match.
  {
    SetUseLayoutDelegate(app_browser, true);
    EXPECT_EQ(toolbar_bounds, GetBoundsInWindow(toolbar, browser_view))
        << "Toolbar bounds differ.";
  }
}

IN_PROC_BROWSER_TEST_F(BrowserViewLayoutDelegateImplBrowsertest,
                       Screenshot_TabbedBrowser) {
  SetUseLayoutDelegate(browser(), true);

  gfx::Rect bounds;
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Screenshot not supported on all platforms"),
      WithView(kBrowserViewElementId,
               [this, &bounds](BrowserView* browser_view) {
                 TabStrip* const tabstrip = browser_view->tabstrip();
                 tabstrip->InvalidateLayout();
                 views::test::RunScheduledLayout(browser_view);
                 bounds = GetBoundsInWindow(tabstrip, browser_view);
                 bounds.set_x(0);
                 bounds.set_width(browser_view->width());
               }),
      Screenshot(kBrowserViewElementId, "tabstrip_region", "6956029",
                 std::ref(bounds)));
}

// TODO(crbug.com/445725696): Fix failing test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Screenshot_AppBrowser DISABLED_Screenshot_AppBrowser
#else
#define MAYBE_Screenshot_AppBrowser Screenshot_AppBrowser
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewLayoutDelegateImplBrowsertest,
                       MAYBE_Screenshot_AppBrowser) {
  Browser* const app_browser = CreateAppBrowser();
  SetUseLayoutDelegate(app_browser, true);

  gfx::Rect bounds;
  RunTestSequenceInContext(
      BrowserElements::From(app_browser)->GetContext(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Screenshot not supported on all platforms"),
      WithView(kBrowserViewElementId,
               [this, &bounds](BrowserView* browser_view) {
                 WebAppFrameToolbarView* const toolbar =
                     browser_view->web_app_frame_toolbar_for_testing();
                 toolbar->InvalidateLayout();
                 views::test::RunScheduledLayout(browser_view);
                 bounds = GetBoundsInWindow(toolbar, browser_view);
                 bounds.set_x(0);
                 bounds.set_width(browser_view->width());
               }),
      Screenshot(kBrowserViewElementId, "tabstrip_region", "6956029",
                 std::ref(bounds)));
}
