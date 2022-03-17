// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

class IntentChipButtonBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  IntentChipButtonBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{apps::features::kLinkCapturingUiUpdate,
                              apps::features::kIntentChipSkipsPicker},
        /*disabled_features=*/{});
  }

  void OpenNewTab(const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    NavigateToLaunchingPage(browser());
    ClickLinkAndWait(web_contents, url, LinkTarget::SELF, "");
  }

  IntentChipButton* GetIntentChip() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetIntentChipButton();
  }

  void ClickIntentChip() {
    views::test::ButtonTestApi test_api(GetIntentChip());
    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    test_api.NotifyClick(e);
  }

  bool HasRequiredAshVersionForLacros() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // For Lacros tests, we need a version of Ash which is new enough to send
    // Preferred Apps over crosapi.
    if (chromeos::LacrosService::Get()->GetInterfaceVersion(
            crosapi::mojom::AppServiceSubscriber::Uuid_) <
        static_cast<int>(
            crosapi::mojom::AppServiceSubscriber::MethodMinVersions::
                kInitializePreferredAppsMinVersion)) {
      LOG(WARNING) << "Unsupported ash version.";
      return false;
    }
#endif
    return true;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       NavigationToInScopeLinkShowsIntentChip) {
  if (!HasRequiredAshVersionForLacros())
    return;

  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");

  EXPECT_TRUE(GetIntentChip()->GetVisible());

  // Clicking the chip should immediately launch the app.

  ClickIntentChip();

  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       NavigationToOutOfScopeLinkDoesNotShowsIntentChip) {
  if (!HasRequiredAshVersionForLacros())
    return;

  InstallTestWebApp();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ClickLinkAndWait(web_contents, out_of_scope_url, LinkTarget::SELF, "");

  EXPECT_FALSE(GetIntentChip()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  if (!HasRequiredAshVersionForLacros())
    return;

  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  OmniboxChipButton* intent_chip_button = GetIntentChip();

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_chip_button->GetVisible());
  OpenNewTab(out_of_scope_url);
  EXPECT_FALSE(intent_chip_button->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(intent_chip_button->GetVisible());

  chrome::SelectNextTab(browser());
  EXPECT_FALSE(intent_chip_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       ShowsIntentPickerWhenMultipleApps) {
  if (!HasRequiredAshVersionForLacros())
    return;

  InstallTestWebApp();

  // Install a second app with a different start URL and an overlapping scope.
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  const char* app_host = GetAppUrlHost();
  web_app_info->start_url = https_server().GetURL(app_host, "/");
  web_app_info->scope = https_server().GetURL(app_host, "/");
  web_app_info->title = base::UTF8ToUTF16(GetAppName());
  web_app_info->description = u"Test description";
  web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");
  base::RunLoop().RunUntilIdle();

  // The Intent Chip should appear, but the intent picker bubble should not
  // appear automatically.
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(IntentPickerBubbleView::intent_picker_bubble());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  ClickIntentChip();

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(IntentPickerBubbleView::intent_picker_bubble());
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest, ShowsIntentChipCollapsed) {
  if (!HasRequiredAshVersionForLacros())
    return;

  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  const GURL separate_host_url =
      https_server().GetURL(GetLaunchingPageHost(), GetLaunchingPagePath());

  NavigateToLaunchingPage(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 1st appearance: Expanded.
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

  ClickLinkAndWait(web_contents, separate_host_url, LinkTarget::SELF, "");
  EXPECT_FALSE(GetIntentChip()->GetVisible());

  // 2nd appearance: Expanded.
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

  ClickLinkAndWait(web_contents, out_of_scope_url, LinkTarget::SELF, "");
  EXPECT_FALSE(GetIntentChip()->GetVisible());

  // 3rd appearance: Expanded.
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

  ClickLinkAndWait(web_contents, out_of_scope_url, LinkTarget::SELF, "");
  EXPECT_FALSE(GetIntentChip()->GetVisible());

  // 4th appearance: Collapsed.
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_TRUE(GetIntentChip()->is_fully_collapsed());

  // Click to open app and reset the counter.
  ClickIntentChip();

  // Open another browser- we should be able to see the expanded chip again.
  NavigateToLaunchingPage(browser());
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // 1st appearance since intent chip counter reset: Expanded.
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());
}
