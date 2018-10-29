// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/test/test_views.h"

// Tests hosted app windows that use the OpaqueBrowserFrameView implementation
// for their non client frames.
class HostedAppOpaqueBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  HostedAppOpaqueBrowserFrameViewTest() = default;
  ~HostedAppOpaqueBrowserFrameViewTest() override = default;

  static GURL GetAppURL() { return GURL("https://test.org"); }

  bool InstallAndLaunchHostedApp(
      base::Optional<SkColor> theme_color = base::nullopt) {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GetAppURL();
    web_app_info.scope = GetAppURL().GetWithoutFilename();
    web_app_info.theme_color = theme_color;

    const extensions::Extension* app =
        extensions::browsertest_util::InstallBookmarkApp(browser()->profile(),
                                                         web_app_info);
    Browser* app_browser = extensions::browsertest_util::LaunchAppBrowser(
        browser()->profile(), app);

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
    hosted_app_button_container_ =
        opaque_browser_frame_view_->hosted_app_button_container_for_testing();
    DCHECK(hosted_app_button_container_);
    DCHECK(hosted_app_button_container_->visible());

    return true;
  }

  int GetRestoredTitleBarHeight() {
    return opaque_browser_frame_view_->layout()->NonClientTopHeight(true);
  }

  OpaqueBrowserFrameView* opaque_browser_frame_view_ = nullptr;
  HostedAppButtonContainer* hosted_app_button_container_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedAppOpaqueBrowserFrameViewTest);
};

IN_PROC_BROWSER_TEST_F(HostedAppOpaqueBrowserFrameViewTest, NoThemeColor) {
  if (!InstallAndLaunchHostedApp())
    return;
  EXPECT_EQ(hosted_app_button_container_->active_color_for_testing(),
            SK_ColorBLACK);
}

IN_PROC_BROWSER_TEST_F(HostedAppOpaqueBrowserFrameViewTest, LightThemeColor) {
  if (!InstallAndLaunchHostedApp(SK_ColorYELLOW))
    return;
  SkColor dark_yellow = SkColorSetRGB(92, 92, 0);
  EXPECT_EQ(hosted_app_button_container_->active_color_for_testing(),
            dark_yellow);
}

IN_PROC_BROWSER_TEST_F(HostedAppOpaqueBrowserFrameViewTest, DarkThemeColor) {
  if (!InstallAndLaunchHostedApp(SK_ColorBLUE))
    return;
  EXPECT_EQ(hosted_app_button_container_->active_color_for_testing(),
            SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(HostedAppOpaqueBrowserFrameViewTest, MediumThemeColor) {
  // Use the theme color for Gmail.
  if (!InstallAndLaunchHostedApp(SkColorSetRGB(0xd6, 0x49, 0x3b)))
    return;
  EXPECT_EQ(hosted_app_button_container_->active_color_for_testing(),
            SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(HostedAppOpaqueBrowserFrameViewTest,
                       StaticTitleBarHeight) {
  if (!InstallAndLaunchHostedApp())
    return;

  const int title_bar_height = GetRestoredTitleBarHeight();
  EXPECT_GT(title_bar_height, 0);

  // Add taller children to the hosted app button container.
  const int container_height = hosted_app_button_container_->height();
  hosted_app_button_container_->AddChildView(
      new views::StaticSizedView(gfx::Size(1, title_bar_height * 2)));
  opaque_browser_frame_view_->Layout();

  // The height of the hosted app button container and title bar should not be
  // affected.
  EXPECT_EQ(container_height, hosted_app_button_container_->height());
  EXPECT_EQ(title_bar_height, GetRestoredTitleBarHeight());
}
