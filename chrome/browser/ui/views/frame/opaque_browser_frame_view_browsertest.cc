// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/test_views.h"

// Tests web-app windows that use the OpaqueBrowserFrameView implementation
// for their non client frames.
class WebAppOpaqueBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  WebAppOpaqueBrowserFrameViewTest() = default;
  ~WebAppOpaqueBrowserFrameViewTest() override = default;

  static GURL GetAppURL() { return GURL("https://test.org"); }

  void SetUpOnMainThread() override { SetThemeMode(ThemeMode::kDefault); }

  bool InstallAndLaunchWebApp(
      base::Optional<SkColor> theme_color = base::nullopt) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();
    web_app_info->theme_color = theme_color;

    web_app::AppId app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    Browser* app_browser =
        web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

    views::NonClientFrameView* frame_view =
        BrowserView::GetBrowserViewForBrowser(app_browser)
            ->GetWidget()
            ->non_client_view()
            ->frame_view();

    // Not all platform configurations use OpaqueBrowserFrameView for their
    // browser windows, see |CreateBrowserNonClientFrameView()|.
    bool is_opaque_browser_frame_view =
        frame_view->GetClassName() == OpaqueBrowserFrameView::kClassName;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    DCHECK(is_opaque_browser_frame_view);
#else
    if (!is_opaque_browser_frame_view)
      return false;
#endif

    opaque_browser_frame_view_ =
        static_cast<OpaqueBrowserFrameView*>(frame_view);
    web_app_frame_toolbar_ =
        opaque_browser_frame_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());

    return true;
  }

  int GetRestoredTitleBarHeight() {
    return opaque_browser_frame_view_->layout()->NonClientTopHeight(true);
  }

  enum class ThemeMode {
    kSystem,
    kDefault,
  };

  void SetThemeMode(ThemeMode theme_mode) {
    ThemeService* theme_service =
        ThemeServiceFactory::GetForProfile(browser()->profile());
    if (theme_mode == ThemeMode::kSystem)
      theme_service->UseSystemTheme();
    else
      theme_service->UseDefaultTheme();
    ASSERT_EQ(theme_service->UsingDefaultTheme(),
              theme_mode == ThemeMode::kDefault);
  }

  OpaqueBrowserFrameView* opaque_browser_frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppOpaqueBrowserFrameViewTest);
};

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, NoThemeColor) {
  if (!InstallAndLaunchWebApp())
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            gfx::kGoogleGrey900);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, SystemThemeColor) {
  SetThemeMode(ThemeMode::kSystem);
  // The color here should be ignored in system mode.
  ASSERT_TRUE(InstallAndLaunchWebApp(SK_ColorRED));

  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            gfx::kGoogleGrey900);
}
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, LightThemeColor) {
  if (!InstallAndLaunchWebApp(SK_ColorYELLOW))
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            gfx::kGoogleGrey900);
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, DarkThemeColor) {
  if (!InstallAndLaunchWebApp(SK_ColorBLUE))
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(), SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, MediumThemeColor) {
  // Use the theme color for Gmail.
  if (!InstallAndLaunchWebApp(SkColorSetRGB(0xd6, 0x49, 0x3b)))
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(), SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, StaticTitleBarHeight) {
  if (!InstallAndLaunchWebApp())
    return;

  opaque_browser_frame_view_->Layout();
  const int title_bar_height = GetRestoredTitleBarHeight();
  EXPECT_GT(title_bar_height, 0);

  // Add taller children to the web app frame toolbar RHS.
  const int container_height = web_app_frame_toolbar_->height();
  web_app_frame_toolbar_->GetRightContainerForTesting()->AddChildView(
      new views::StaticSizedView(gfx::Size(1, title_bar_height * 2)));
  opaque_browser_frame_view_->Layout();

  // The height of the web app frame toolbar and title bar should not be
  // affected.
  EXPECT_EQ(container_height, web_app_frame_toolbar_->height());
  EXPECT_EQ(title_bar_height, GetRestoredTitleBarHeight());
}
