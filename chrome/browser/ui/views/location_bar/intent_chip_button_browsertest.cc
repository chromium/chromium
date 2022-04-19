// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  void TearDownOnMainThread() override {
    web_app::test::UninstallWebApp(profile(), test_web_app_id());
    if (!overlapping_app_id_.empty()) {
      web_app::test::UninstallWebApp(profile(), overlapping_app_id_);
    }
    web_app::WebAppNavigationBrowserTest::TearDownOnMainThread();
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
            crosapi::mojom::AppServiceProxy::Uuid_) <
        static_cast<int>(crosapi::mojom::AppServiceProxy::MethodMinVersions::
                             kAddPreferredAppMinVersion)) {
      LOG(WARNING) << "Unsupported ash version.";
      return false;
    }
#endif
    return true;
  }

  // Installs a web app on the same host as InstallTestWebApp(), but with "/" as
  // a scope, so it overlaps with all URLs in the test app scope.
  void InstallOverlappingApp() {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    const char* app_host = GetAppUrlHost();
    web_app_info->start_url = https_server().GetURL(app_host, "/");
    web_app_info->scope = https_server().GetURL(app_host, "/");
    web_app_info->title = base::UTF8ToUTF16(GetAppName());
    web_app_info->description = u"Test description";
    web_app_info->user_display_mode = web_app::UserDisplayMode::kStandalone;

    overlapping_app_id_ =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::AppId overlapping_app_id_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       NavigationToInScopeLinkShowsIntentChip) {
  if (!HasRequiredAshVersionForLacros())
    GTEST_SKIP() << "Ash version is too old to support Intent Picker";

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
    GTEST_SKIP() << "Ash version is too old to support Intent Picker";

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
    GTEST_SKIP() << "Ash version is too old to support Intent Picker";

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

// TODO(crbug.com/1313274): Fix test flakiness on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ShowsIntentPickerWhenMultipleApps \
  DISABLED_ShowsIntentPickerWhenMultipleApps
#else
#define MAYBE_ShowsIntentPickerWhenMultipleApps \
  ShowsIntentPickerWhenMultipleApps
#endif
IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       MAYBE_ShowsIntentPickerWhenMultipleApps) {
  if (!HasRequiredAshVersionForLacros())
    GTEST_SKIP() << "Ash version is too old to support Intent Picker";

  InstallTestWebApp();
  InstallOverlappingApp();

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
    GTEST_SKIP() << "Ash version is too old to support Intent Picker";

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

class IntentChipButtonIPHBubbleBrowserTest
    : public IntentChipButtonBrowserTest {
 public:
  IntentChipButtonIPHBubbleBrowserTest() {
    feature_list_.InitAndEnableFeature(
        feature_engagement::kIPHIntentChipFeature);
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &IntentChipButtonIPHBubbleBrowserTest::RegisterTestTracker));
  }

 private:
  static void RegisterTestTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestTracker));
  }
  static std::unique_ptr<KeyedService> CreateTestTracker(
      content::BrowserContext*) {
    return feature_engagement::CreateTestTracker();
  }

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonIPHBubbleBrowserTest, ShowAndCloseIPH) {
  if (!HasRequiredAshVersionForLacros())
    GTEST_SKIP() << "Ash version is too old to support Intent Picker";

  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  BrowserFeaturePromoController* const promo_controller =
      static_cast<BrowserFeaturePromoController*>(
          browser()->window()->GetFeaturePromoController());
  feature_engagement::Tracker* tracker =
      promo_controller->feature_engagement_tracker();
  base::RunLoop loop;
  tracker->AddOnInitializedCallback(
      base::BindLambdaForTesting([&loop](bool success) {
        DCHECK(success);
        loop.Quit();
      }));
  loop.Run();
  ASSERT_TRUE(tracker->IsInitialized());

  NavigateToLaunchingPage(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to an in-scope page to see the intent chip and the IPH.
  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");
  EXPECT_TRUE(GetIntentChip()->GetVisible());

  // Check if the IPH bubble is showing.
  EXPECT_TRUE(promo_controller->IsPromoActive(
      feature_engagement::kIPHIntentChipFeature));

  // When we click on the intent chip, the IPH should disappear.
  ClickIntentChip();

  // Check the IPH is no longer showing.
  EXPECT_FALSE(promo_controller->IsPromoActive(
      feature_engagement::kIPHIntentChipFeature));
}

class IntentChipButtonAppIconBrowserTest : public IntentChipButtonBrowserTest {
 public:
  IntentChipButtonAppIconBrowserTest() {
    feature_list_.InitAndEnableFeature(apps::features::kIntentChipAppIcon);
  }

  void ClickLinkAndWaitForIconUpdate(content::WebContents* web_contents,
                                     const GURL& link_url) {
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(web_contents);
    base::RunLoop run_loop;
    tab_helper->SetIconUpdateCallbackForTesting(
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

    ClickLinkAndWait(web_contents, link_url, LinkTarget::SELF, "");

    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonAppIconBrowserTest, ShowsAppIconInChip) {
  if (!HasRequiredAshVersionForLacros())
    GTEST_SKIP() << "Ash version is too old to support Intent Picker";

  InstallTestWebApp();
  InstallOverlappingApp();

  const GURL root_url = https_server().GetURL(GetAppUrlHost(), "/");
  const GURL overlapped_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL non_overlapped_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ClickLinkAndWaitForIconUpdate(web_contents, root_url);

  auto icon1 =
      GetIntentChip()->GetImage(views::Button::ButtonState::STATE_NORMAL);
  ASSERT_FALSE(IntentPickerTabHelper::FromWebContents(web_contents)
                   ->app_icon()
                   .IsEmpty());

  ClickLinkAndWaitForIconUpdate(web_contents, non_overlapped_url);

  // The chip should still be showing the same app icon.
  auto icon2 =
      GetIntentChip()->GetImage(views::Button::ButtonState::STATE_NORMAL);
  ASSERT_TRUE(icon1.BackedBySameObjectAs(icon2));

  ClickLinkAndWaitForIconUpdate(web_contents, overlapped_url);

  // Loading a URL with multiple apps available should switch to a generic icon.
  auto icon3 =
      GetIntentChip()->GetImage(views::Button::ButtonState::STATE_NORMAL);
  ASSERT_FALSE(icon1.BackedBySameObjectAs(icon3));
  ASSERT_TRUE(IntentPickerTabHelper::FromWebContents(web_contents)
                  ->app_icon()
                  .IsEmpty());
}
