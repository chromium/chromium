// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/views/view_utils.h"

class WebAppGlassBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  WebAppGlassBrowserFrameViewTest() = default;
  WebAppGlassBrowserFrameViewTest(const WebAppGlassBrowserFrameViewTest&) =
      delete;
  WebAppGlassBrowserFrameViewTest& operator=(
      const WebAppGlassBrowserFrameViewTest&) = delete;
  ~WebAppGlassBrowserFrameViewTest() override = default;

  GURL GetStartURL() { return GURL("https://test.org"); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    WebAppToolbarButtonContainer::DisableAnimationForTesting();
  }

  // Windows 7 does not use GlassBrowserFrameView when Aero glass is not
  // enabled. Skip testing in this scenario.
  // TODO(https://crbug.com/863278): Force Aero glass on Windows 7 for this
  // test.
  bool InstallAndLaunchWebApp() {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = GetStartURL();
    web_app_info->scope = GetStartURL().GetWithoutFilename();
    if (theme_color_)
      web_app_info->theme_color = *theme_color_;

    web_app::AppId app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetStartURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    if (!views::IsViewClass<GlassBrowserFrameView>(frame_view))
      return false;
    glass_frame_view_ = static_cast<GlassBrowserFrameView*>(frame_view);

    web_app_frame_toolbar_ =
        glass_frame_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());
    return true;
  }

  base::Optional<SkColor> theme_color_ = SK_ColorBLUE;
  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  GlassBrowserFrameView* glass_frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, ThemeColor) {
  if (!InstallAndLaunchWebApp())
    return;

  EXPECT_EQ(glass_frame_view_->GetTitlebarColor(), *theme_color_);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, NoThemeColor) {
  theme_color_ = base::nullopt;
  if (!InstallAndLaunchWebApp())
    return;

  EXPECT_EQ(glass_frame_view_->GetTitlebarColor(),
            ThemeProperties::GetDefaultColor(
                ThemeProperties::COLOR_FRAME_ACTIVE, false));
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, MaximizedLayout) {
  if (!InstallAndLaunchWebApp())
    return;

  glass_frame_view_->frame()->Maximize();
  static_cast<views::View*>(glass_frame_view_)->Layout();

  DCHECK_GT(glass_frame_view_->window_title_for_testing()->x(), 0);
  DCHECK_GT(glass_frame_view_->web_app_frame_toolbar_for_testing()->y(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, RTLTopRightHitTest) {
  base::i18n::SetRTLForTesting(true);
  if (!InstallAndLaunchWebApp())
    return;

  static_cast<views::View*>(glass_frame_view_)->Layout();

  // Avoid the top right resize corner.
  constexpr int kInset = 10;
  EXPECT_EQ(glass_frame_view_->NonClientHitTest(
                gfx::Point(glass_frame_view_->width() - kInset, kInset)),
            HTCAPTION);
}

class WebAppGlassBrowserFrameViewWindowControlsOverlayTest
    : public InProcessBrowserTest {
 public:
  WebAppGlassBrowserFrameViewWindowControlsOverlayTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        features::kWebAppWindowControlsOverlay);
  }
  WebAppGlassBrowserFrameViewWindowControlsOverlayTest(
      const WebAppGlassBrowserFrameViewWindowControlsOverlayTest&) = delete;
  WebAppGlassBrowserFrameViewWindowControlsOverlayTest& operator=(
      const WebAppGlassBrowserFrameViewWindowControlsOverlayTest&) = delete;

  ~WebAppGlassBrowserFrameViewWindowControlsOverlayTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  GURL LoadTestPageWithDataAndGetURL() {
    // Write |data| to a temporary file that can be later reached at
    // http://127.0.0.1/test_file_*.html.
    static int s_test_file_number = 1;

    const char kTestHTML[] =
        "<!DOCTYPE html>"
        "<style>"
        "  #target {"
        "    -webkit-app-region: drag;"
        "     height: 100px;"
        "     width: 100px;"
        "  }"
        "</style>"
        "<div id=target></div>";

    base::FilePath file_path = temp_dir_.GetPath().AppendASCII(
        base::StringPrintf("test_file_%d.html", s_test_file_number++));

    base::ScopedAllowBlockingForTesting allow_temp_file_writing;
    base::WriteFile(file_path, kTestHTML);

    GURL url = embedded_test_server()->GetURL(
        "/" + file_path.BaseName().AsUTF8Unsafe());

    return url;
  }

  bool InstallAndLaunchWebAppWithWindowControlsOverlay() {
    GURL start_url = LoadTestPageWithDataAndGetURL();

    std::vector<blink::mojom::DisplayMode> display_overrides;
    display_overrides.emplace_back(
        blink::mojom::DisplayMode::kWindowControlsOverlay);
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->open_as_window = true;
    web_app_info->title = u"A Web App";
    web_app_info->display_override = display_overrides;

    web_app::AppId app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));

    content::TestNavigationObserver navigation_observer(start_url);
    base::RunLoop loop;
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

    // TODO(crbug.com/1191186): Register binder for BrowserInterfaceBroker
    // during testing.
    app_browser_->app_controller()->SetOnUpdateDraggableRegionForTesting(
        loop.QuitClosure());
    web_app::NavigateToURLAndWait(app_browser_, start_url);
    loop.Run();
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    if (!views::IsViewClass<GlassBrowserFrameView>(frame_view))
      return false;

    glass_frame_view_ = static_cast<GlassBrowserFrameView*>(frame_view);
    web_app_frame_toolbar_ =
        glass_frame_view_->web_app_frame_toolbar_for_testing();

    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());
    return true;
  }

  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  GlassBrowserFrameView* glass_frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       WindowControlsOverlayDraggableRegions) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  static_cast<views::View*>(glass_frame_view_)->Layout();

  constexpr gfx::Point kPoint(50, 50);
  EXPECT_EQ(glass_frame_view_->NonClientHitTest(kPoint), HTCAPTION);
  EXPECT_FALSE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetNativeWindow(), kPoint));
}
