// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_navigation_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_zoom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#endif

namespace {

#if BUILDFLAG(IS_MAC)
// Keep in sync with browser_non_client_frame_view_mac.mm
constexpr double kTitlePaddingWidthFraction = 0.1;
#endif

template <typename T>
T* GetLastVisible(const std::vector<T*>& views) {
  T* visible = nullptr;
  for (auto* view : views) {
    if (view->GetVisible()) {
      visible = view;
    }
  }
  return visible;
}

void LoadTestPopUpExtension(Profile* profile) {
  extensions::TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      R"({
          "name": "Pop up extension",
          "version": "1.0",
          "manifest_version": 2,
          "browser_action": {
            "default_popup": "popup.html"
          }
         })");
  test_extension_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), "");
  extensions::ChromeTestExtensionLoader(profile).LoadExtension(
      test_extension_dir.UnpackedPath());
}

}  // namespace

class WebAppFrameToolbarBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // WebAppControllerBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    WebAppControllerBrowserTest::SetUp();
  }

  WebAppFrameToolbarTestHelper* helper() {
    return &web_app_frame_toolbar_helper_;
  }

  bool IsMenuCommandEnabled(int command_id) {
    auto app_menu_model = std::make_unique<WebAppMenuModel>(
        /*provider=*/nullptr, helper()->app_browser());
    app_menu_model->Init();
    ui::MenuModel* model = app_menu_model.get();
    size_t index = 0;
    return app_menu_model->GetModelAndIndexForCommandId(command_id, &model,
                                                        &index) &&
           model->IsEnabledAt(index);
  }

 private:
  net::EmbeddedTestServer https_server_;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, SpaceConstrained) {
  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  WebAppNavigationButtonContainer* const toolbar_left_container =
      helper()->web_app_frame_toolbar()->get_left_container_for_testing();
  EXPECT_EQ(toolbar_left_container->parent(),
            helper()->web_app_frame_toolbar());

  views::View* const window_title =
      helper()->frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(window_title);
#else
  EXPECT_EQ(window_title->parent(), helper()->browser_view()->top_container());
#endif

  WebAppToolbarButtonContainer* const toolbar_right_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();
  EXPECT_EQ(toolbar_right_container->parent(),
            helper()->web_app_frame_toolbar());

  std::vector<const PageActionIconView*> page_actions =
      helper()
          ->web_app_frame_toolbar()
          ->GetPageActionIconControllerForTesting()
          ->GetPageActionIconViewsForTesting();
  for (const PageActionIconView* action : page_actions) {
    EXPECT_EQ(action->parent(), toolbar_right_container);
  }

  views::View* const menu_button =
      helper()->browser_view()->toolbar_button_provider()->GetAppMenuButton();
  EXPECT_EQ(menu_button->parent(), toolbar_right_container);

  // Ensure we initially have abundant space. Set the size from the root view
  // which will get propagated to the frame view.
  helper()->root_view()->SetSize(gfx::Size(1000, 1000));

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  const int original_left_container_width = toolbar_left_container->width();
  EXPECT_GT(original_left_container_width, 0);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
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
  RunScheduledLayouts();

  // The page action icons should now take up width, leaving less space on
  // Windows and Linux for the window title. (On Mac, the window title remains
  // centered - not tested here.)

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  EXPECT_EQ(toolbar_left_container->width(), original_left_container_width);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
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
  // Set the size of the desired frame width from the root view.
  helper()->root_view()->SetSize(
      {new_frame_width, helper()->root_view()->height()});

  // The left container (containing Back and Reload) should be hidden.
  EXPECT_FALSE(toolbar_left_container->GetVisible());

  // The window title should be clipped to 0 width.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
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

#if !BUILDFLAG(IS_LINUX)
  // Avoid dependence on Linux GTK+ Themes appearance setting.

  ToolbarButtonProvider* const toolbar_button_provider =
      helper()->browser_view()->toolbar_button_provider();
  AppMenuButton* const app_menu_button =
      toolbar_button_provider->GetAppMenuButton();

  auto get_ink_drop_color = [app_menu_button]() -> SkColor {
    return SkColorSetA(views::InkDrop::Get(app_menu_button)->GetBaseColor(),
                       SK_AlphaOPAQUE);
  };

  const SkColor original_ink_drop_color = get_ink_drop_color();

  // Change the theme-color.
  {
    content::ThemeChangeWaiter theme_change_waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents,
                                "document.getElementById('theme-color')."
                                "setAttribute('content', '#246')"));
    theme_change_waiter.Wait();

    EXPECT_NE(get_ink_drop_color(), original_ink_drop_color);
  }

  // Change the theme-color back to its original one.
  {
    content::ThemeChangeWaiter theme_change_waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents,
                                "document.getElementById('theme-color')."
                                "setAttribute('content', '#ace')"));
    theme_change_waiter.Wait();

    EXPECT_EQ(get_ink_drop_color(), original_ink_drop_color);
  }
#endif
}

// Test that a tooltip is shown when hovering over a truncated title.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, TitleHover) {
  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  auto* const window_title = static_cast<views::Label*>(
      helper()->frame_view()->GetViewByID(VIEW_ID_WINDOW_TITLE));
#if BUILDFLAG(IS_CHROMEOS)
  // Chrome OS PWA windows do not display app titles.
  EXPECT_EQ(nullptr, window_title);
  return;
#else
  WebAppNavigationButtonContainer* const toolbar_left_container =
      helper()->web_app_frame_toolbar()->get_left_container_for_testing();
  WebAppToolbarButtonContainer* const toolbar_right_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  EXPECT_EQ(window_title->parent(), helper()->browser_view()->top_container());
  window_title->SetText(std::u16string(30, 't'));

  // Ensure we initially have abundant space. Set the size from the root view
  // which will get propagated to the frame view.
  helper()->root_view()->SetSize(gfx::Size(1000, 1000));
  EXPECT_GT(window_title->width(), 0);
  const int original_title_gap = toolbar_right_container->x() -
                                 toolbar_left_container->x() -
                                 toolbar_left_container->width();

  // With a narrow window, we have insufficient space for the full title.
  const int narrow_title_gap =
      window_title->CalculatePreferredSize().width() * 3 / 4;
  int narrow_width =
      helper()->frame_view()->width() - original_title_gap + narrow_title_gap;
#if BUILDFLAG(IS_MAC)
  // Increase width to allow for title padding.
  narrow_width = base::checked_cast<int>(
      std::ceil(narrow_width / (1 - 2 * kTitlePaddingWidthFraction)));
#endif
  helper()->root_view()->SetSize(gfx::Size(narrow_width, 1000));

  EXPECT_GT(window_title->width(), 0);
  EXPECT_EQ(window_title->GetTooltipHandlerForPoint(gfx::Point(0, 0)),
            window_title);

  gfx::Point origin_in_frame_view = views::View::ConvertPointToTarget(
      window_title->parent(), helper()->frame_view(), window_title->origin());
  EXPECT_EQ(
      helper()->frame_view()->GetTooltipHandlerForPoint(origin_in_frame_view),
      window_title);
#endif
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest,
                       MenuButtonAccessibleName) {
  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  views::View* const menu_button =
      helper()->browser_view()->toolbar_button_provider()->GetAppMenuButton();

  EXPECT_EQ(menu_button->GetAccessibleName(),
            u"Customize and control A minimal-ui app");
  EXPECT_EQ(menu_button->GetTooltipText(gfx::Point()),
            u"Customize and control A minimal-ui app");
}

class WebAppFrameToolbarBrowserTest_ElidedExtensionsMenu
    : public WebAppFrameToolbarBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest_ElidedExtensionsMenu() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsElidedExtensionsMenu);
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_ElidedExtensionsMenu,
                       Test) {
  helper()->InstallAndLaunchWebApp(browser(), GURL("https://test.org"));

  // There should be no menu entry for opening the Extensions menu prior to
  // installing Extensions.
  EXPECT_FALSE(IsMenuCommandEnabled(WebAppMenuModel::kExtensionsMenuCommandId));

  // Install test Extension.
  LoadTestPopUpExtension(browser()->profile());

  // There should be no visible Extensions icon.
  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();
  EXPECT_FALSE(toolbar_button_container->extensions_container()->GetVisible());

  // There should be a menu entry for opening the Extensions menu.
  EXPECT_TRUE(IsMenuCommandEnabled(WebAppMenuModel::kExtensionsMenuCommandId));

  // Trigger the Extensions menu entry.
  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, helper()->app_browser());
  app_menu_model->Init();
  app_menu_model->ExecuteCommand(WebAppMenuModel::kExtensionsMenuCommandId,
                                 /*event_flags=*/0);

  // Extensions icon and menu should be visible.
  EXPECT_TRUE(toolbar_button_container->extensions_container()->GetVisible());
  EXPECT_TRUE(ExtensionsMenuView::IsShowing());
}

class WebAppFrameToolbarBrowserTest_NoElidedExtensionsMenu
    : public WebAppFrameToolbarBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest_NoElidedExtensionsMenu() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDesktopPWAsElidedExtensionsMenu);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_NoElidedExtensionsMenu,
                       Test) {
  helper()->InstallAndLaunchWebApp(browser(), GURL("https://test.org"));

  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  // Extensions toolbar should be hidden while there are no Extensions
  // installed.
  EXPECT_FALSE(toolbar_button_container->extensions_container()->GetVisible());

  // Install Extension and wait for Extensions toolbar to appear.
  base::RunLoop run_loop;
  ExtensionsToolbarContainer::SetOnVisibleCallbackForTesting(
      run_loop.QuitClosure());
  LoadTestPopUpExtension(browser()->profile());
  run_loop.Run();
  EXPECT_TRUE(toolbar_button_container->extensions_container()->GetVisible());

  // There should be no menu entry for opening the Extensions menu.
  EXPECT_FALSE(IsMenuCommandEnabled(WebAppMenuModel::kExtensionsMenuCommandId));
}

// Borderless has not been implemented for win/mac.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
class BorderlessIsolatedWebAppBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  BorderlessIsolatedWebAppBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppBorderless);
  }

  void InstallAndLaunchIsolatedWebApp(bool uses_borderless) {
    isolated_web_app_dev_server_ = CreateAndStartServer(
        FILE_PATH_LITERAL(uses_borderless ? "web_apps/borderless_isolated_app"
                                          : "web_apps/simple_isolated_app"));
    web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
        isolated_web_app_dev_server().GetOrigin());
    browser_ = GetBrowserFromFrame(OpenApp(url_info.app_id()));
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser_);

    if (uses_borderless) {
      // In web_apps/borderless_isolated_app/borderless.js the title is set on
      // `window.onload`. This is to make sure that the web contents have loaded
      // before doing any checks and to reduce the flakiness of the tests.
      content::TitleWatcher title_watcher(
          browser_view()->GetActiveWebContents(), kBorderlessAppOnloadTitle);
      EXPECT_EQ(title_watcher.WaitAndGetTitle(), kBorderlessAppOnloadTitle);
    }
    EXPECT_EQ(uses_borderless, browser_view()->AppUsesBorderlessMode());

    views::NonClientFrameView* frame_view =
        browser_view()->GetWidget()->non_client_view()->frame_view();
    frame_view_ = static_cast<BrowserNonClientFrameView*>(frame_view);
  }

  void GrantWindowManagementPermission() {
    auto* web_contents = browser_view()->GetActiveWebContents();
    std::string permission_auto_approve_script = R"(
      const draggable = document.getElementById('draggable');
      draggable.setAttribute('allow', 'window-management');
    )";
    EXPECT_TRUE(ExecJs(web_contents, permission_auto_approve_script,
                       content::EXECUTE_SCRIPT_NO_USER_GESTURE));

    permissions::PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);

    EXPECT_TRUE(ExecJs(web_contents, "window.getScreenDetails();"));

    std::string permission_query_script = R"(
      navigator.permissions.query({
        name: 'window-management'
      }).then(res => res.state)
    )";
    EXPECT_EQ("granted", EvalJs(web_contents, permission_query_script));

    // It takes some time to udate the borderless mode state. The title is
    // updated on a change event hooked to the window.matchMedia() function,
    // which gets triggered when the permission is granted and the borderless
    // mode gets enabled.
    const std::u16string kExpectedMatchMediaTitle = u"match-media-borderless";
    content::TitleWatcher title_watcher(web_contents, kExpectedMatchMediaTitle);
    ASSERT_EQ(title_watcher.WaitAndGetTitle(), kExpectedMatchMediaTitle);
  }

  BorderlessIsolatedWebAppBrowserTest(
      const BorderlessIsolatedWebAppBrowserTest&) = delete;
  BorderlessIsolatedWebAppBrowserTest& operator=(
      const BorderlessIsolatedWebAppBrowserTest&) = delete;

 protected:
  // This string must match with the title set in the `window.onload` function
  // in web_apps/borderless_isolated_app/borderless.js.
  const std::u16string kBorderlessAppOnloadTitle = u"Borderless";

  BrowserView* browser_view() { return browser_view_; }

  WebAppFrameToolbarView* web_app_frame_toolbar() {
    return browser_view()->web_app_frame_toolbar_for_testing();
  }

  BrowserNonClientFrameView* frame_view() { return frame_view_; }

  const net::EmbeddedTestServer& isolated_web_app_dev_server() {
    return *isolated_web_app_dev_server_.get();
  }

  // Opens a new popup window from `browser_` by running
  // `window_open_script` and returns the `BrowserView` of the popup it opened.
  BrowserView* OpenPopup(const std::string& window_open_script) {
    content::ExecuteScriptAsync(browser_view_->GetActiveWebContents(),
                                window_open_script);
    Browser* popup = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_NE(browser_, popup);
    EXPECT_TRUE(popup);

    BrowserView* popup_browser_view =
        BrowserView::GetBrowserViewForBrowser(popup);
    EXPECT_TRUE(content::WaitForRenderFrameReady(
        popup_browser_view->GetActiveWebContents()->GetPrimaryMainFrame()));

    return popup_browser_view;
  }

  bool IsWindowSizeCorrect(BrowserView* browser_view,
                           gfx::Size& expected_inner_size,
                           gfx::Size& expected_outer_size) {
    auto* web_contents = browser_view->GetActiveWebContents();

    const auto& client_view_size = browser_view->frame()->client_view()->size();

    return client_view_size.height() == expected_inner_size.height() &&
           client_view_size.width() == expected_inner_size.width() &&
           EvalJs(web_contents, "window.innerHeight").ExtractInt() ==
               expected_inner_size.height() &&
           EvalJs(web_contents, "window.outerHeight").ExtractInt() ==
               expected_outer_size.height() &&
           EvalJs(web_contents, "window.innerWidth").ExtractInt() ==
               expected_inner_size.width() &&
           EvalJs(web_contents, "window.outerWidth").ExtractInt() ==
               expected_outer_size.width();
  }

  void WaitForWindowSizeCorrectlyUpdated(BrowserView* browser_view,
                                         gfx::Size& expected_inner_size,
                                         gfx::Size& expected_outer_size) {
    auto* web_contents = browser_view->GetActiveWebContents();

    while (!(web_contents->CompletedFirstVisuallyNonEmptyPaint() &&
             !web_contents->IsLoading() &&
             web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame() &&
             IsWindowSizeCorrect(browser_view, expected_inner_size,
                                 expected_outer_size))) {
      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
      run_loop.Run();
    }

    ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_;
  raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_;
  raw_ptr<BrowserNonClientFrameView, AcrossTasksDanglingUntriaged> frame_view_;
};

IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest,
                       AppUsesBorderlessModeAndHasWindowManagementPermission) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  GrantWindowManagementPermission();

  ASSERT_TRUE(
      browser_view()->window_management_permission_granted_for_testing());
  ASSERT_TRUE(browser_view()->IsBorderlessModeEnabled());
}

IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest,
                       DisplayModeMediaCSS) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  auto* web_contents = browser_view()->GetActiveWebContents();

  std::string get_background_color = R"(
    window.getComputedStyle(document.body, null)
      .getPropertyValue('background-color');
  )";
  std::string match_media_standalone =
      "window.matchMedia('(display-mode: standalone)').matches;";
  std::string match_media_borderless =
      "window.matchMedia('(display-mode: borderless)').matches;";
  std::string blue = "rgb(0, 0, 255)";
  std::string red = "rgb(255, 0, 0)";

  // Validate that before granting the permission, the display-mode matches with
  // the default value "standalone" and the default background-color.
  EXPECT_TRUE(EvalJs(web_contents, match_media_standalone).ExtractBool());
  ASSERT_EQ(blue, EvalJs(web_contents, get_background_color));

  GrantWindowManagementPermission();
  ASSERT_TRUE(browser_view()->IsBorderlessModeEnabled());

  // Validate that after granting the permission the display-mode matches with
  // "borderless" and updates the background-color accordingly.
  EXPECT_TRUE(EvalJs(web_contents, match_media_borderless).ExtractBool());
  ASSERT_EQ(red, EvalJs(web_contents, get_background_color));
}

IN_PROC_BROWSER_TEST_F(
    BorderlessIsolatedWebAppBrowserTest,
    AppUsesBorderlessModeAndDoesNotHaveWindowManagementPermission) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  ASSERT_TRUE(browser_view()->borderless_mode_enabled_for_testing());
  ASSERT_FALSE(
      browser_view()->window_management_permission_granted_for_testing());
  ASSERT_FALSE(browser_view()->IsBorderlessModeEnabled());
}

IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest,
                       AppDoesntUseBorderlessMode) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/false);
  ASSERT_FALSE(browser_view()->borderless_mode_enabled_for_testing());
  ASSERT_FALSE(
      browser_view()->window_management_permission_granted_for_testing());
  ASSERT_FALSE(browser_view()->IsBorderlessModeEnabled());
}

IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest,
                       PopupToItselfIsBorderless) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  GrantWindowManagementPermission();
  ASSERT_TRUE(browser_view()->IsBorderlessModeEnabled());

  // Popup to itself.
  auto url =
      EvalJs(browser_view()->GetActiveWebContents(), "window.location.href")
          .ExtractString();
  BrowserView* popup_browser_view =
      OpenPopup("window.open('" + url + "', '_blank', 'popup');");
  EXPECT_TRUE(popup_browser_view->IsBorderlessModeEnabled());
}

IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest,
                       PopupToAnyOtherOriginIsNotBorderless) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  GrantWindowManagementPermission();
  ASSERT_TRUE(browser_view()->IsBorderlessModeEnabled());

  // Popup to any other website outside of the same origin.
  BrowserView* popup_browser_view =
      OpenPopup("window.open('https://google.com', '_blank', 'popup');");
  EXPECT_FALSE(popup_browser_view->IsBorderlessModeEnabled());
}

IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest, PopupSize) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  GrantWindowManagementPermission();
  ASSERT_TRUE(browser_view()->IsBorderlessModeEnabled());

  auto url =
      EvalJs(browser_view()->GetActiveWebContents(), "window.location.href")
          .ExtractString();

  BrowserView* popup_browser_view =
      OpenPopup("window.open('" + url +
                "', '', 'location=0, status=0, scrollbars=0, "
                "left=0, top=0, width=400, height=300');");
  EXPECT_TRUE(popup_browser_view->IsBorderlessModeEnabled());
  auto* popup_web_contents = popup_browser_view->GetActiveWebContents();

  // Make sure the popup is fully ready. The title gets set to Borderless on
  // window.onload event.
  content::TitleWatcher init_title_watcher(popup_web_contents,
                                           kBorderlessAppOnloadTitle);
  EXPECT_EQ(init_title_watcher.WaitAndGetTitle(), kBorderlessAppOnloadTitle);

  constexpr int kExpectedWidth = 400, kExpectedHeight = 300;
  gfx::Size expected_size(kExpectedWidth, kExpectedHeight);

// For ChromeOS the resizable borders are "outside of the window" where as for
// Linux they are "inside of the window".
#if BUILDFLAG(IS_CHROMEOS)
  WaitForWindowSizeCorrectlyUpdated(popup_browser_view, expected_size,
                                    expected_size);
#elif BUILDFLAG(IS_LINUX)
  constexpr int kFrameInsets =
      2 * OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
  // window.open() sets the inner size to match with the given size.
  gfx::Size expected_outer_size(kExpectedWidth + kFrameInsets,
                                kExpectedHeight + kFrameInsets);
  WaitForWindowSizeCorrectlyUpdated(popup_browser_view, expected_size,
                                    expected_outer_size);
#endif
}

IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest, PopupResize) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  GrantWindowManagementPermission();
  ASSERT_TRUE(browser_view()->IsBorderlessModeEnabled());

  auto url =
      EvalJs(browser_view()->GetActiveWebContents(), "window.location.href")
          .ExtractString();

  BrowserView* popup_browser_view =
      OpenPopup("window.open('" + url +
                "', '', 'location=0, status=0, scrollbars=0, "
                "left=0, top=0, width=400, height=300');");

  EXPECT_TRUE(popup_browser_view->IsBorderlessModeEnabled());
  auto* popup_web_contents = popup_browser_view->GetActiveWebContents();

  // Make sure the popup is fully ready. The title gets set to Borderless on
  // window.onload event.
  content::TitleWatcher init_title_watcher(popup_web_contents,
                                           kBorderlessAppOnloadTitle);
  EXPECT_EQ(init_title_watcher.WaitAndGetTitle(), kBorderlessAppOnloadTitle);

  const std::u16string kResizeTitle = u"resized";
  content::TitleWatcher resized_title_watcher(popup_web_contents, kResizeTitle);

  const std::string kOnResizeScript = content::JsReplace(R"(
    document.title = 'beforeevent';
    window.onresize = (e) => {
      document.title = $1;
    }
  )",
                                                         kResizeTitle);

  EXPECT_TRUE(ExecJs(popup_web_contents, kOnResizeScript));
  EXPECT_TRUE(ExecJs(popup_web_contents, "window.resizeTo(600,500)"));
  std::ignore = resized_title_watcher.WaitAndGetTitle();
  EXPECT_EQ(popup_web_contents->GetTitle(), kResizeTitle);

  constexpr int kExpectedWidth = 600, kExpectedHeight = 500;
  gfx::Size expected_size(kExpectedWidth, kExpectedHeight);

#if BUILDFLAG(IS_CHROMEOS)
  WaitForWindowSizeCorrectlyUpdated(popup_browser_view, expected_size,
                                    expected_size);
#elif BUILDFLAG(IS_LINUX)
  constexpr int kFrameInsets =
      2 * OpaqueBrowserFrameViewLayout::kFrameBorderThickness;
  // window.resizeTo() sets the outer size to match with the given size.
  gfx::Size expected_inner_size(kExpectedWidth - kFrameInsets,
                                kExpectedHeight - kFrameInsets);
  WaitForWindowSizeCorrectlyUpdated(popup_browser_view, expected_inner_size,
                                    expected_size);
#endif
}

// Test to ensure that the minimum size for a borderless app is as small as
// possible. To test the fix for b/265935069.
IN_PROC_BROWSER_TEST_F(BorderlessIsolatedWebAppBrowserTest, FrameMinimumSize) {
  InstallAndLaunchIsolatedWebApp(/*uses_borderless=*/true);
  GrantWindowManagementPermission();

  ASSERT_TRUE(browser_view()->borderless_mode_enabled_for_testing());
  ASSERT_TRUE(
      browser_view()->window_management_permission_granted_for_testing());
  ASSERT_TRUE(browser_view()->IsBorderlessModeEnabled());

  // The minimum size of a window is smaller for a borderless mode app than for
  // a normal app. The size of the borders is inconsistent (and we don't have
  // access to the exact borders from here) and varies by OS.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_LT(frame_view()->GetMinimumSize().width(),
            BrowserViewLayout::kMainBrowserContentsMinimumWidth);
#elif BUILDFLAG(IS_LINUX)
  EXPECT_EQ(frame_view()->GetMinimumSize(), gfx::Size(1, 1));
#endif
}
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

class WebAppFrameToolbarBrowserTest_WindowControlsOverlay
    : public WebAppFrameToolbarBrowserTest {
 public:
  class TestInfoBarDelegate : public infobars::InfoBarDelegate {
   public:
    static infobars::InfoBar* Create(
        infobars::ContentInfoBarManager* infobar_manager) {
      return static_cast<InfoBarView*>(
          infobar_manager->AddInfoBar(std::make_unique<InfoBarView>(
              std::make_unique<TestInfoBarDelegate>())));
    }

    TestInfoBarDelegate() = default;
    ~TestInfoBarDelegate() override = default;

    // infobars::InfoBarDelegate:
    infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier()
        const override {
      return InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
    }
  };

  WebAppFrameToolbarBrowserTest_WindowControlsOverlay() {
    scoped_feature_list_.InitAndEnableFeature(safe_browsing::kDownloadBubble);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
    WebAppFrameToolbarBrowserTest::SetUp();
  }

  webapps::AppId InstallAndLaunchWCOWebApp(GURL start_url,
                                           std::u16string app_title) {
    std::vector<blink::mojom::DisplayMode> display_overrides;
    display_overrides.push_back(web_app::DisplayMode::kWindowControlsOverlay);
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = std::move(app_title);
    web_app_info->display_mode = web_app::DisplayMode::kStandalone;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->display_override = display_overrides;

    return helper()->InstallAndLaunchCustomWebApp(
        browser(), std::move(web_app_info), start_url);
  }

  webapps::AppId InstallAndLaunchWebApp() {
    EXPECT_TRUE(https_server()->Start());
    return InstallAndLaunchWCOWebApp(
        helper()->LoadWindowControlsOverlayTestPageWithDataAndGetURL(
            embedded_test_server(), &temp_dir_),
        u"A window-controls-overlay app");
  }

  webapps::AppId InstallAndLaunchFullyDraggableWebApp() {
    EXPECT_TRUE(https_server()->Start());
    return InstallAndLaunchWCOWebApp(
        helper()->LoadWholeAppIsDraggableTestPageWithDataAndGetURL(
            embedded_test_server(), &temp_dir_),
        u"Full page draggable window-controls-overlay app");
  }

  void ToggleWindowControlsOverlayAndWaitHelper(
      content::WebContents* web_contents,
      BrowserView* browser_view) {
    helper()->SetupGeometryChangeCallback(web_contents);
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    base::test::TestFuture<void> future;
    browser_view->ToggleWindowControlsOverlayEnabled(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  // When toggling the WCO app initialized by the helper class.
  void ToggleWindowControlsOverlayAndWait() {
    ToggleWindowControlsOverlayAndWaitHelper(
        helper()->browser_view()->GetActiveWebContents(),
        helper()->browser_view());
  }

  bool GetWindowControlOverlayVisibility() {
    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    return EvalJs(web_contents,
                  "window.navigator.windowControlsOverlay.visible")
        .ExtractBool();
  }

  bool GetWindowControlOverlayVisibilityFromEvent() {
    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    return EvalJs(web_contents, "overlay_visible_from_event").ExtractBool();
  }

  void ShowInfoBarAndWait() {
    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    helper()->SetupGeometryChangeCallback(web_contents);
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    TestInfoBarDelegate::Create(
        infobars::ContentInfoBarManager::FromWebContents(
            helper()
                ->app_browser()
                ->tab_strip_model()
                ->GetActiveWebContents()));
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  gfx::Rect GetWindowControlOverlayBoundingClientRect() {
    const std::string kRectValueList =
        "var rect = "
        "[navigator.windowControlsOverlay.getTitlebarAreaRect().x, "
        "navigator.windowControlsOverlay.getTitlebarAreaRect().y, "
        "navigator.windowControlsOverlay.getTitlebarAreaRect().width, "
        "navigator.windowControlsOverlay.getTitlebarAreaRect().height];";
    return helper()->GetXYWidthHeightRect(
        helper()->browser_view()->GetActiveWebContents(), kRectValueList,
        "rect");
  }

  std::string GetCSSTitlebarRect() {
    return "var element = document.getElementById('target');"
           "var titlebarAreaX = "
           "    getComputedStyle(element).getPropertyValue('padding-left');"
           "var titlebarAreaXInt = parseInt(titlebarAreaX.split('px')[0]);"
           "var titlebarAreaY = "
           "    getComputedStyle(element).getPropertyValue('padding-top');"
           "var titlebarAreaYInt = parseInt(titlebarAreaY.split('px')[0]);"
           "var titlebarAreaWidthRect = "
           "    getComputedStyle(element).getPropertyValue('padding-right');"
           "var titlebarAreaWidthRectInt = "
           "    parseInt(titlebarAreaWidthRect.split('px')[0]);"
           "var titlebarAreaHeightRect = "
           "    getComputedStyle(element).getPropertyValue('padding-bottom');"
           "var titlebarAreaHeightRectInt = "
           "    parseInt(titlebarAreaHeightRect.split('px')[0]);";
  }

  void ResizeWindowBoundsAndWait(const gfx::Rect& new_bounds) {
    // Changing the width of widget should trigger a "geometrychange" event.
    EXPECT_NE(new_bounds.width(),
              helper()->browser_view()->GetLocalBounds().width());

    auto* web_contents = helper()->browser_view()->GetActiveWebContents();
    helper()->SetupGeometryChangeCallback(web_contents);
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    helper()->browser_view()->GetWidget()->SetBounds(new_bounds);
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  gfx::Rect GetWindowControlOverlayBoundingClientRectFromEvent() {
    const std::string kRectValueList =
        "var rect = [overlay_rect_from_event.x, overlay_rect_from_event.y, "
        "overlay_rect_from_event.width, overlay_rect_from_event.height];";

    return helper()->GetXYWidthHeightRect(
        helper()->browser_view()->GetActiveWebContents(), kRectValueList,
        "rect");
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       WindowControlsOverlay) {
  InstallAndLaunchWebApp();

  // Toggle overlay on, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayAndWait();

  gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());

#if BUILDFLAG(IS_MAC)
  EXPECT_NE(0, bounds.x());
  EXPECT_EQ(0, bounds.y());
#else
  EXPECT_EQ(gfx::Point(), bounds.origin());
#endif
  EXPECT_FALSE(bounds.IsEmpty());

  // Toggle overlay off, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayAndWait();
  bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_FALSE(GetWindowControlOverlayVisibility());
  EXPECT_EQ(gfx::Rect(), bounds);
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       GeometryChangeEvent) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  // Store the initial bounding client rect for comparison later.
  const gfx::Rect initial_js_overlay_bounds =
      GetWindowControlOverlayBoundingClientRect();
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() - 1);
  ResizeWindowBoundsAndWait(new_bounds);

  // Validate both the event payload and JS bounding client rect reflect
  // the new size.
  const gfx::Rect resized_js_overlay_bounds =
      GetWindowControlOverlayBoundingClientRect();
  const gfx::Rect resized_js_overlay_event_bounds =
      GetWindowControlOverlayBoundingClientRectFromEvent();
  EXPECT_EQ(1, EvalJs(helper()->browser_view()->GetActiveWebContents(),
                      "geometrychangeCount"));
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_TRUE(GetWindowControlOverlayVisibilityFromEvent());
  EXPECT_EQ(resized_js_overlay_bounds, resized_js_overlay_event_bounds);
  EXPECT_EQ(initial_js_overlay_bounds.origin(),
            resized_js_overlay_bounds.origin());
  EXPECT_NE(initial_js_overlay_bounds.width(),
            resized_js_overlay_bounds.width());
  EXPECT_EQ(initial_js_overlay_bounds.height(),
            resized_js_overlay_bounds.height());
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       NoGeometryChangeEventIfOverlayIsOff) {
  InstallAndLaunchWebApp();

  constexpr char kTestScript[] =
      "document.title = 'beforeevent';"
      "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
      "  document.title = 'ongeometrychange';"
      "};"
      "window.onresize = (e) => {"
      "  document.title = 'onresize';"
      "};";

  // Window Controls Overlay is off by default.
  ASSERT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());

  auto* web_contents = helper()->browser_view()->GetActiveWebContents();
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 10);
  content::TitleWatcher title_watcher(web_contents, u"onresize");
  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), kTestScript));
  helper()->browser_view()->GetWidget()->SetBounds(new_bounds);
  title_watcher.AlsoWaitForTitle(u"ongeometrychange");
  EXPECT_EQ(u"onresize", title_watcher.WaitAndGetTitle());

  // Toggle Window Control Overlay on and then off.
  ToggleWindowControlsOverlayAndWait();
  ToggleWindowControlsOverlayAndWait();

  // Validate event is not fired.
  new_bounds.set_width(new_bounds.width() - 10);
  content::TitleWatcher title_watcher2(web_contents, u"onresize");
  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), kTestScript));
  helper()->browser_view()->GetWidget()->SetBounds(new_bounds);
  title_watcher2.AlsoWaitForTitle(u"ongeometrychange");
  EXPECT_EQ(u"onresize", title_watcher2.WaitAndGetTitle());
}

// TODO(crbug.com/1306499): Enable for mac/win when flakiness has been fixed.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
// Test to ensure crbug.com/1298226 won't reproduce.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       PopupFromWcoAppToItself) {
  InstallAndLaunchWebApp();
  auto* wco_web_contents = helper()->browser_view()->GetActiveWebContents();

  // Popup to itself.
  auto url = EvalJs(wco_web_contents, "window.location.href").ExtractString();
  BrowserView* popup_browser_view =
      helper()->OpenPopup("window.open('" + url + "', '_blank', 'popup');");
  content::WebContents* popup_web_contents =
      popup_browser_view->GetActiveWebContents();
  EXPECT_FALSE(popup_browser_view->IsWindowControlsOverlayEnabled());
  EXPECT_FALSE(EvalJs(popup_web_contents,
                      "window.navigator.windowControlsOverlay.visible")
                   .ExtractBool());

  // When popup is opened (from a WCO app) pointing to itself, the popup also
  // has WCO which can be toggled.
  ToggleWindowControlsOverlayAndWaitHelper(popup_web_contents,
                                           popup_browser_view);
  EXPECT_TRUE(popup_browser_view->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(EvalJs(popup_web_contents,
                     "window.navigator.windowControlsOverlay.visible")
                  .ExtractBool());
}

// Test to ensure crbug.com/1298237 won't reproduce.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       PopupFromWcoAppToAnyOtherWebsite) {
  InstallAndLaunchWebApp();
  // The initial WCO state doesn't matter, but to highlight that it's different,
  // the script is run with the WCO initially toggled on.
  ToggleWindowControlsOverlayAndWait();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());

  // Popup to any other website outside of the same origin, and wait
  // for the page to load.
  ui_test_utils::UrlLoadObserver observer(
      GURL("https://google.com"), content::NotificationService::AllSources());
  BrowserView* popup_browser_view = helper()->OpenPopup(
      "window.open('https://google.com', '_blank', 'popup');");
  observer.Wait();

  // When popup is opened pointing to any other site, it will not know whether
  // the popup app uses WCO or not. This test also ensures it does not crash.
  EXPECT_FALSE(popup_browser_view->IsWindowControlsOverlayEnabled());
  EXPECT_FALSE(EvalJs(popup_browser_view->GetActiveWebContents(),
                      "window.navigator.windowControlsOverlay.visible")
                   .ExtractBool());
}
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       WindowControlsOverlayRTL) {
  base::test::ScopedRestoreICUDefaultLocale test_locale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  const gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_NE(0, bounds.x());
  EXPECT_EQ(0, bounds.y());
  EXPECT_FALSE(bounds.IsEmpty());
}

// Test to ensure crbug.com/1353133 won't reproduce. It casts the frame_view to
// the ChromeOS's frame_view to have access to the caption_button_container_ so
// it cannot be run on any other platform.
#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       WindowControlsOverlayFrameViewHeight) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());

  BrowserNonClientFrameViewChromeOS* frame_view_cros =
      static_cast<BrowserNonClientFrameViewChromeOS*>(helper()->frame_view());

  int frame_view_height = frame_view_cros->GetMinimumSize().height();
  int caption_container_height =
      frame_view_cros->caption_button_container()->size().height();
  int client_view_height =
      frame_view_cros->frame()->client_view()->GetMinimumSize().height();

  EXPECT_EQ(frame_view_height, caption_container_height + client_view_height);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       CSSRectTestLTR) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  std::string kCSSTitlebarRect = GetCSSTitlebarRect();
  auto* web_contents = helper()->browser_view()->GetActiveWebContents();
  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), kCSSTitlebarRect));

  const std::string kRectListString =
      "var rect = [titlebarAreaXInt, titlebarAreaYInt, "
      "titlebarAreaWidthRectInt, "
      "titlebarAreaHeightRectInt];";

  base::Value::List initial_rect_list = helper()->GetXYWidthHeightListValue(
      helper()->browser_view()->GetActiveWebContents(), kRectListString,
      "rect");

  const int initial_x_value = initial_rect_list[0].GetInt();
  const int initial_y_value = initial_rect_list[1].GetInt();
  const int initial_width_value = initial_rect_list[2].GetInt();
  const int initial_height_value = initial_rect_list[3].GetInt();

#if BUILDFLAG(IS_MAC)
  // Window controls are on the opposite side on Mac.
  EXPECT_NE(0, initial_x_value);
#else
  EXPECT_EQ(0, initial_x_value);
#endif
  EXPECT_EQ(0, initial_y_value);
  EXPECT_NE(0, initial_width_value);
  EXPECT_NE(0, initial_height_value);

  // Change bounds so new values get sent.
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 20);
  new_bounds.set_height(new_bounds.height() + 20);
  ResizeWindowBoundsAndWait(new_bounds);

  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), kCSSTitlebarRect));

  base::Value::List updated_rect_list = helper()->GetXYWidthHeightListValue(
      helper()->browser_view()->GetActiveWebContents(), kRectListString,
      "rect");

  // Changing the window dimensions should only change the overlay width. The
  // overlay height should remain the same.
  EXPECT_EQ(initial_x_value, updated_rect_list[0].GetInt());
  EXPECT_EQ(initial_y_value, updated_rect_list[1].GetInt());
  EXPECT_NE(initial_width_value, updated_rect_list[2].GetInt());
  EXPECT_EQ(initial_height_value, updated_rect_list[3].GetInt());
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       CSSRectTestRTL) {
  base::test::ScopedRestoreICUDefaultLocale test_locale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  std::string kCSSTitlebarRect = GetCSSTitlebarRect();
  auto* web_contents = helper()->browser_view()->GetActiveWebContents();
  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), kCSSTitlebarRect));

  const std::string kRectListString =
      "var rect = [titlebarAreaXInt, titlebarAreaYInt, "
      "titlebarAreaWidthRectInt, "
      "titlebarAreaHeightRectInt];";

  base::Value::List initial_rect_list = helper()->GetXYWidthHeightListValue(
      helper()->browser_view()->GetActiveWebContents(), kRectListString,
      "rect");

  const int initial_x_value = initial_rect_list[0].GetInt();
  const int initial_y_value = initial_rect_list[1].GetInt();
  const int initial_width_value = initial_rect_list[2].GetInt();
  const int initial_height_value = initial_rect_list[3].GetInt();

  EXPECT_NE(0, initial_x_value);
  EXPECT_EQ(0, initial_y_value);
  EXPECT_NE(0, initial_width_value);
  EXPECT_NE(0, initial_height_value);

  // Change bounds so new values get sent.
  gfx::Rect new_bounds = helper()->browser_view()->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 15);
  new_bounds.set_height(new_bounds.height() + 15);
  ResizeWindowBoundsAndWait(new_bounds);

  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), kCSSTitlebarRect));

  base::Value::List updated_rect_list = helper()->GetXYWidthHeightListValue(
      helper()->browser_view()->GetActiveWebContents(), kRectListString,
      "rect");

  // Changing the window dimensions should only change the overlay width. The
  // overlay height should remain the same.
  EXPECT_EQ(initial_x_value, updated_rect_list[0].GetInt());
  EXPECT_EQ(initial_y_value, updated_rect_list[1].GetInt());
  EXPECT_NE(initial_width_value, updated_rect_list[2].GetInt());
  EXPECT_EQ(initial_height_value, updated_rect_list[3].GetInt());
}

// TODO(https://crbug.com/1277860): Flaky. Also enable for borderless mode when
// fixed.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       DISABLED_WindowControlsOverlayDraggableRegions) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();
  helper()->TestDraggableRegions();
}

// Regression test for https://crbug.com/1448878.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       DraggableRegionsIgnoredForOwnedWidgets) {
  auto app_id = InstallAndLaunchFullyDraggableWebApp();
  ToggleWindowControlsOverlayAndWait();

  BrowserView* browser_view = helper()->browser_view();
  views::NonClientFrameView* frame_view =
      browser_view->GetWidget()->non_client_view()->frame_view();

  // A widget owned by BrowserView is triggered to ensure that a click inside
  // the widget overlaying a draggable region correctly returns `HTCLIENT` and
  // not `HTCAPTION`. The widget ownership varies between platforms so using
  // different widgets based on platform.

#if BUILDFLAG(IS_WIN)
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "DropdownBarHost");
  // Press Ctrl+F to open find bar.
  content::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_F;
  event.skip_if_unhandled = false;
  browser_view->GetActiveWebContents()
      ->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEvent(event);
#else
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "PermissionPromptBubbleBaseView");
  content::ExecuteScriptAsyncWithoutUserGesture(
      browser_view->GetActiveWebContents(),
      "navigator.geolocation.getCurrentPosition(() => {});");
#endif  // BUILDFLAG(IS_WIN)

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  // A point inside the widget is not draggable and returns `HTCLIENT` and not
  // e.g. `HTCAPTION`.
  auto widget_in_screen_bounds = widget->GetWindowBoundsInScreen();
  gfx::Point point_in_widget = widget_in_screen_bounds.CenterPoint();
  views::View::ConvertPointToTarget(
      browser_view, browser_view->contents_web_view(), &point_in_widget);
  EXPECT_TRUE(browser_view->ShouldDescendIntoChildForEventHandling(
      browser_view->GetWidget()->GetNativeView(), point_in_widget));
  EXPECT_EQ(frame_view->NonClientHitTest(point_in_widget), HTCLIENT);

  // A point inside a draggable region (but outside the widget) is draggable
  // and returns `HTCAPTION` as expected. This is to make sure having the widget
  // open doesn't interfere with the way the draggable regions work beyond the
  // area of the widget.
  gfx::Point point_below_widget =
      gfx::Point(widget_in_screen_bounds.bottom_center().x(),
                 widget_in_screen_bounds.bottom_center().y() + 5);
  EXPECT_FALSE(browser_view->ShouldDescendIntoChildForEventHandling(
      browser_view->GetWidget()->GetNativeView(), point_below_widget));
  EXPECT_EQ(frame_view->NonClientHitTest(point_below_widget), HTCAPTION);
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       ToggleWindowControlsOverlay) {
  InstallAndLaunchWebApp();

  // Make sure the app launches in standalone mode by default.
  EXPECT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());

  // Toggle WCO on, and verify that the UI updates accordingly.
  ToggleWindowControlsOverlayAndWait();
  EXPECT_TRUE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());

  // Toggle WCO off, and verify that the app returns to 'standalone' mode.
  ToggleWindowControlsOverlayAndWait();
  EXPECT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       OpenInChrome) {
  InstallAndLaunchWebApp();

  // Toggle overlay on, and validate JS API reflects the expected values.
  ToggleWindowControlsOverlayAndWait();

  // Validate non-empty bounds are being sent.
  EXPECT_TRUE(GetWindowControlOverlayVisibility());

  chrome::ExecuteCommand(helper()->browser_view()->browser(),
                         IDC_OPEN_IN_CHROME);

  // Validate bounds are cleared.
  EXPECT_EQ(false, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                          "window.navigator.windowControlsOverlay.visible"));
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       HideToggleButtonWhenCCTIsVisible) {
  InstallAndLaunchWebApp();
  EXPECT_TRUE(helper()->browser_view()->AppUsesWindowControlsOverlay());

  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  // Start with app in standalone mode.
  EXPECT_FALSE(helper()->browser_view()->IsWindowControlsOverlayEnabled());
  // Ensure the CCT is hidden before running checks.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ false,
                                                         /*animate*/ false);

  // Verify that the WCO toggle button shows when app is in standalone mode.
  EXPECT_TRUE(toolbar_button_container->window_controls_overlay_toggle_button()
                  ->GetVisible());

  // Show CCT and verify the toggle button hides.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ true,
                                                         /*animate*/ false);
  EXPECT_FALSE(toolbar_button_container->window_controls_overlay_toggle_button()
                   ->GetVisible());

  // Hide CCT and enable window controls overlay.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ false,
                                                         /*animate*/ false);
  ToggleWindowControlsOverlayAndWait();

  // Verify that the app entered window controls overlay mode.
  EXPECT_TRUE(helper()->browser_view()->IsWindowControlsOverlayEnabled());

  // Verify that the WCO toggle button shows when app is in WCO mode.
  EXPECT_TRUE(toolbar_button_container->window_controls_overlay_toggle_button()
                  ->GetVisible());

  // Show CCT and verify the toggle button hides.
  helper()->browser_view()->UpdateCustomTabBarVisibility(/*visible*/ true,
                                                         /*animate*/ false);
  EXPECT_FALSE(toolbar_button_container->window_controls_overlay_toggle_button()
                   ->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       HideToggleButtonWhenInfoBarIsVisible) {
  InstallAndLaunchWebApp();

  BrowserView* browser_view = helper()->browser_view();
  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();

  // Start with app in Window Controls Overlay (WCO) mode and verify that the
  // toggle button is visible.
  ToggleWindowControlsOverlayAndWait();
  EXPECT_TRUE(browser_view->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(toolbar_button_container->window_controls_overlay_toggle_button()
                  ->GetVisible());

  // Show InfoBar and verify the toggle button hides.
  ShowInfoBarAndWait();
  EXPECT_FALSE(toolbar_button_container->window_controls_overlay_toggle_button()
                   ->GetVisible());
  EXPECT_FALSE(browser_view->IsWindowControlsOverlayEnabled());
}

// Regression test for https://crbug.com/1239443.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       OpenWithOverlayEnabled) {
  webapps::AppId app_id = InstallAndLaunchWebApp();
  base::test::TestFuture<void> future;
  helper()
      ->browser_view()
      ->browser()
      ->app_controller()
      ->ToggleWindowControlsOverlayEnabled(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);
  // If there's no crash, the test has passed.
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       DraggableRegionNotResetByFencedFrameNavigation) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  BrowserView* browser_view = helper()->browser_view();
  views::NonClientFrameView* frame_view =
      browser_view->GetWidget()->non_client_view()->frame_view();

  gfx::Point draggable_point(100, 100);
  views::View::ConvertPointToTarget(browser_view->contents_web_view(),
                                    frame_view, &draggable_point);

  // Create a fenced frame and ensure that draggable region doesn't clear after
  // the fenced frame navigation.
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_helper_.CreateFencedFrame(
          browser_view->GetActiveWebContents()->GetPrimaryMainFrame(),
          fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);
  EXPECT_FALSE(browser_view->ShouldDescendIntoChildForEventHandling(
      browser_view->GetWidget()->GetNativeView(), draggable_point));
}

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       FencedFrame) {
  InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  BrowserView* browser_view = helper()->browser_view();
  gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_FALSE(bounds.IsEmpty());
  EXPECT_NE(0, bounds.width());
  EXPECT_NE(0, bounds.height());

  // Ensure window controls overlay values are not sent to a fenced frame.
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");

  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_helper_.CreateFencedFrame(
          browser_view->GetActiveWebContents()->GetPrimaryMainFrame(),
          fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);

  EXPECT_EQ(false, EvalJs(fenced_frame_rfh,
                          "window.navigator.windowControlsOverlay.visible"));
  EXPECT_EQ(
      0,
      EvalJs(fenced_frame_rfh,
             "window.navigator.windowControlsOverlay.getTitlebarAreaRect().x"));
  EXPECT_EQ(
      0,
      EvalJs(fenced_frame_rfh,
             "window.navigator.windowControlsOverlay.getTitlebarAreaRect().y"));
  EXPECT_EQ(0, EvalJs(fenced_frame_rfh,
                      "window.navigator.windowControlsOverlay."
                      "getTitlebarAreaRect().width"));
  EXPECT_EQ(0, EvalJs(fenced_frame_rfh,
                      "window.navigator.windowControlsOverlay."
                      "getTitlebarAreaRect().height"));
}

// Extensions in  ChromeOS are not in the titlebar.
#if !BUILDFLAG(IS_CHROMEOS)
// Regression test for https://crbug.com/1351566.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       ExtensionsIconVisibility) {
  webapps::AppId app_id = InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  // There should be no visible Extensions icon.
  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();
  EXPECT_FALSE(toolbar_button_container->extensions_container()->GetVisible());

  LoadTestPopUpExtension(browser()->profile());

  EXPECT_TRUE(toolbar_button_container->extensions_container()->GetVisible());

  // Shut down the browser with window controls overlay toggled on so for next
  // launch it stays toggled on.
  CloseBrowserSynchronously(helper()->app_browser());

  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  auto* web_app_frame_toolbar =
      browser_view->web_app_frame_toolbar_for_testing();

  // There should be a visible Extensions icon.
  EXPECT_TRUE(web_app_frame_toolbar->get_right_container_for_testing()
                  ->extensions_container()
                  ->GetVisible());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Test that a download by a web app browser only shows the download UI in that
// app's window.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       DownloadIconVisibilityForAppDownload) {
  webapps::AppId app_id = InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  Browser* non_app_browser = CreateBrowser(profile());

  // There should be no visible Downloads icon prior to the download, in either
  // the app browser or the non-app browser.
  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();
  EXPECT_FALSE(toolbar_button_container->download_button()->GetVisible());
  EXPECT_FALSE(non_app_browser->window()
                   ->GetDownloadBubbleUIController()
                   ->GetDownloadDisplayController()
                   ->download_display_for_testing()
                   ->IsShowing());

  // Download a file in the app browser.
  ui_test_utils::DownloadURL(
      helper()->app_browser(),
      ui_test_utils::GetTestUrl(
          base::FilePath().AppendASCII("downloads"),
          base::FilePath().AppendASCII("a_zip_file.zip")));

  // The download button is visible in the app browser.
  EXPECT_TRUE(toolbar_button_container->download_button()->GetVisible());

  // The download button is not visible in the non-app browser.
  EXPECT_FALSE(non_app_browser->window()
                   ->GetDownloadBubbleUIController()
                   ->GetDownloadDisplayController()
                   ->download_display_for_testing()
                   ->IsShowing());
}

// Test that a download by a regular browser does not show the download UI in an
// app's window.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       DownloadIconVisibilityForRegularDownload) {
  webapps::AppId app_id = InstallAndLaunchWebApp();
  ToggleWindowControlsOverlayAndWait();

  Browser* non_app_browser = CreateBrowser(profile());

  // There should be no visible Downloads icon prior to the download, in either
  // the app browser or the non-app browser.
  WebAppToolbarButtonContainer* toolbar_button_container =
      helper()->web_app_frame_toolbar()->get_right_container_for_testing();
  EXPECT_FALSE(toolbar_button_container->download_button()->GetVisible());
  EXPECT_FALSE(non_app_browser->window()
                   ->GetDownloadBubbleUIController()
                   ->GetDownloadDisplayController()
                   ->download_display_for_testing()
                   ->IsShowing());

  // Download a file in the regular browser.
  ui_test_utils::DownloadURL(
      non_app_browser, ui_test_utils::GetTestUrl(
                           base::FilePath().AppendASCII("downloads"),
                           base::FilePath().AppendASCII("a_zip_file.zip")));

  // The download button is not visible in the app browser.
  EXPECT_FALSE(toolbar_button_container->download_button()->GetVisible());

  // The download button is visible in the non-app browser.
  EXPECT_TRUE(non_app_browser->window()
                  ->GetDownloadBubbleUIController()
                  ->GetDownloadDisplayController()
                  ->download_display_for_testing()
                  ->IsShowing());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest_WindowControlsOverlay,
                       DisplayModeMediaCSS) {
  InstallAndLaunchWebApp();
  auto* web_contents = helper()->browser_view()->GetActiveWebContents();

  std::string get_background_color = R"(
    window.getComputedStyle(document.body, null)
      .getPropertyValue('background-color');
  )";
  std::string match_media_standalone =
      "window.matchMedia('(display-mode: standalone)').matches;";
  std::string match_media_wco =
      "window.matchMedia('(display-mode: window-controls-overlay)').matches;";
  std::string blue = "rgb(0, 0, 255)";
  std::string red = "rgb(255, 0, 0)";

  // Initially launches with WCO off. Validate the display-mode matches with the
  // default value "standalone" and the default background-color.
  EXPECT_FALSE(GetWindowControlOverlayVisibility());
  ASSERT_TRUE(EvalJs(web_contents, match_media_standalone).ExtractBool());
  ASSERT_EQ(blue, EvalJs(web_contents, get_background_color));

  // Toggle WCO on, and validate the display-mode matches with
  // "window-controls-overlay" and updates the background-color.
  ToggleWindowControlsOverlayAndWait();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  ASSERT_TRUE(EvalJs(web_contents, match_media_wco).ExtractBool());
  ASSERT_EQ(red, EvalJs(web_contents, get_background_color));

  // Toggle WCO back off and ensure it updates to be the same as in the
  // beginning.
  ToggleWindowControlsOverlayAndWait();
  EXPECT_FALSE(GetWindowControlOverlayVisibility());
  ASSERT_TRUE(EvalJs(web_contents, match_media_standalone).ExtractBool());
  ASSERT_EQ(blue, EvalJs(web_contents, get_background_color));
}
