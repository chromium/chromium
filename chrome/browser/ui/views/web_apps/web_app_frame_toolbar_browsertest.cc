// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/optional.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_zoom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

#if defined(OS_MAC)
// Keep in sync with browser_non_client_frame_view_mac.mm
constexpr double kTitlePaddingWidthFraction = 0.1;
#endif

template <typename T>
T* GetLastVisible(const std::vector<T*>& views) {
  T* visible = nullptr;
  for (auto* view : views) {
    if (view->GetVisible())
      visible = view;
  }
  return visible;
}

}  // namespace

class WebAppFrameToolbarBrowserTest : public InProcessBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    InProcessBrowserTest::SetUp();
  }

  WebAppFrameToolbarTestHelper* helper() {
    return &web_app_frame_toolbar_helper_;
  }

 private:
  net::EmbeddedTestServer https_server_;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, SpaceConstrained) {
  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  views::View* const toolbar_left_container =
      helper()->web_app_frame_toolbar()->GetLeftContainerForTesting();
  EXPECT_EQ(toolbar_left_container->parent(),
            helper()->web_app_frame_toolbar());

  views::View* const window_title =
      helper()->frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE);
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(window_title);
#else
  EXPECT_EQ(window_title->parent(), helper()->frame_view());
#endif

  views::View* const toolbar_right_container =
      helper()->web_app_frame_toolbar()->GetRightContainerForTesting();
  EXPECT_EQ(toolbar_right_container->parent(),
            helper()->web_app_frame_toolbar());

  std::vector<const PageActionIconView*> page_actions =
      helper()
          ->web_app_frame_toolbar()
          ->GetPageActionIconControllerForTesting()
          ->GetPageActionIconViewsForTesting();
  for (const PageActionIconView* action : page_actions)
    EXPECT_EQ(action->parent(), toolbar_right_container);

  views::View* const menu_button =
      helper()->browser_view()->toolbar_button_provider()->GetAppMenuButton();
  EXPECT_EQ(menu_button->parent(), toolbar_right_container);

  // Ensure we initially have abundant space.
  helper()->frame_view()->SetSize(gfx::Size(1000, 1000));

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  const int original_left_container_width = toolbar_left_container->width();
  EXPECT_GT(original_left_container_width, 0);

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  const int original_window_title_width = window_title->width();
  EXPECT_GT(original_window_title_width, 0);
#endif

  // Initially the page action icons are not visible.
  EXPECT_EQ(GetLastVisible(page_actions), nullptr);
  const int original_menu_button_width = menu_button->width();
  EXPECT_GT(original_menu_button_width, 0);

  // Cause the zoom page action icon to be visible.
  chrome::Zoom(helper()->app_browser(), content::PAGE_ZOOM_IN);

  // The layout should be invalidated, but since we don't have the benefit of
  // the compositor to immediately kick a layout off, we have to do it manually.
  helper()->frame_view()->Layout();

  // The page action icons should now take up width, leaving less space on
  // Windows and Linux for the window title. (On Mac, the window title remains
  // centered - not tested here.)

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  EXPECT_EQ(toolbar_left_container->width(), original_left_container_width);

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  EXPECT_GT(window_title->width(), 0);
  EXPECT_LT(window_title->width(), original_window_title_width);
#endif

  EXPECT_NE(GetLastVisible(page_actions), nullptr);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);

  // Resize the WebAppFrameToolbarView just enough to clip out the page action
  // icons (and toolbar contents left of them).
  const int original_toolbar_width = helper()->web_app_frame_toolbar()->width();
  const int new_toolbar_width = toolbar_right_container->width() -
                                GetLastVisible(page_actions)->bounds().right();
  const int new_frame_width = helper()->frame_view()->width() -
                              original_toolbar_width + new_toolbar_width;

  helper()->web_app_frame_toolbar()->SetSize(
      {new_toolbar_width, helper()->web_app_frame_toolbar()->height()});
  helper()->frame_view()->SetSize(
      {new_frame_width, helper()->frame_view()->height()});

  // The left container (containing Back and Reload) should be hidden.
  EXPECT_FALSE(toolbar_left_container->GetVisible());

  // The window title should be clipped to 0 width.
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  EXPECT_EQ(window_title->width(), 0);
#endif

  // The page action icons should be hidden while the app menu button retains
  // its full width.
  EXPECT_EQ(GetLastVisible(page_actions), nullptr);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, ThemeChange) {
  ASSERT_TRUE(https_server()->Start());
  const GURL app_url = https_server()->GetURL("/banners/theme-color.html");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  content::WebContents* web_contents =
      helper()->app_browser()->tab_strip_model()->GetActiveWebContents();
  content::AwaitDocumentOnLoadCompleted(web_contents);

#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Avoid dependence on Linux GTK+ Themes appearance setting.

  ToolbarButtonProvider* const toolbar_button_provider =
      helper()->browser_view()->toolbar_button_provider();
  AppMenuButton* const app_menu_button =
      toolbar_button_provider->GetAppMenuButton();

  const SkColor original_ink_drop_color =
      app_menu_button->GetInkDropBaseColor();

  {
    content::ThemeChangeWaiter theme_change_waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents,
                                "document.getElementById('theme-color')."
                                "setAttribute('content', '#246')"));
    theme_change_waiter.Wait();

    EXPECT_NE(app_menu_button->GetInkDropBaseColor(), original_ink_drop_color);
  }

  {
    content::ThemeChangeWaiter theme_change_waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(
        web_contents, "document.getElementById('theme-color').remove()"));
    theme_change_waiter.Wait();

    EXPECT_EQ(app_menu_button->GetInkDropBaseColor(), original_ink_drop_color);
  }
#endif
}

// Test that a tooltip is shown when hovering over a truncated title.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, TitleHover) {
  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  views::View* const toolbar_left_container =
      helper()->web_app_frame_toolbar()->GetLeftContainerForTesting();
  views::View* const toolbar_right_container =
      helper()->web_app_frame_toolbar()->GetRightContainerForTesting();

  auto* const window_title = static_cast<views::Label*>(
      helper()->frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE));
#if defined(OS_CHROMEOS)
  // Chrome OS PWA windows do not display app titles.
  EXPECT_EQ(nullptr, window_title);
  return;
#endif
  EXPECT_EQ(window_title->parent(), helper()->frame_view());

  window_title->SetText(base::string16(30, 't'));

  // Ensure we initially have abundant space.
  helper()->frame_view()->SetSize(gfx::Size(1000, 1000));
  helper()->frame_view()->Layout();
  EXPECT_GT(window_title->width(), 0);
  const int original_title_gap = toolbar_right_container->x() -
                                 toolbar_left_container->x() -
                                 toolbar_left_container->width();

  // With a narrow window, we have insufficient space for the full title.
  const int narrow_title_gap =
      window_title->CalculatePreferredSize().width() * 3 / 4;
  int narrow_frame_width =
      helper()->frame_view()->width() - original_title_gap + narrow_title_gap;
#if defined(OS_MAC)
  // Increase frame width to allow for title padding.
  narrow_frame_width = base::checked_cast<int>(
      std::ceil(narrow_frame_width / (1 - 2 * kTitlePaddingWidthFraction)));
#endif
  helper()->frame_view()->SetSize(gfx::Size(narrow_frame_width, 1000));
  helper()->frame_view()->Layout();

  EXPECT_GT(window_title->width(), 0);
  EXPECT_EQ(window_title->GetTooltipHandlerForPoint(gfx::Point(0, 0)),
            window_title);

  EXPECT_EQ(
      helper()->frame_view()->GetTooltipHandlerForPoint(window_title->origin()),
      window_title);
}
