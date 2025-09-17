// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout_delegate_impl.h"

#include <memory>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_utils.h"
#include "url/gurl.h"

class BrowserViewLayoutDelegateImplBrowsertest : public InProcessBrowserTest {
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

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(BrowserViewLayoutDelegateImplBrowsertest,
                       CompareOldAndNewLayout_TabbedBrowser) {
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

IN_PROC_BROWSER_TEST_F(BrowserViewLayoutDelegateImplBrowsertest,
                       CompareOldAndNewLayout_AppBrowser) {
  const GURL kAppUrl("https://test.com");
  const auto app_id = web_app::test::InstallDummyWebApp(browser()->profile(),
                                                        "App Name", kAppUrl);
  Browser* const app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
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
