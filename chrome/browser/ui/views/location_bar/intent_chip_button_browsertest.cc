// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
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
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/services/app_service/public/cpp/features.h"
#endif

namespace {

// ViewObserver that sends a callback when the target View's size is set to a
// nonzero value.
class NonzeroSizeObserver : public views::ViewObserver {
 public:
  NonzeroSizeObserver(views::View* view, base::OnceClosure callback)
      : callback_(std::move(callback)) {
    if (!view->size().IsEmpty())
      std::move(callback_).Run();
    else
      observation_.Observe(view);
  }
  ~NonzeroSizeObserver() override = default;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    if (!observed_view->size().IsEmpty()) {
      std::move(callback_).Run();
      observation_.Reset();
    }
  }

 private:
  base::OnceClosure callback_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

}  // namespace

class IntentChipButtonBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  IntentChipButtonBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{apps::features::kLinkCapturingUiUpdate},
        /*disabled_features=*/{apps::features::kIntentChipSkipsPicker,
                               apps::features::kIntentChipAppIcon});
  }

  void SetUpOnMainThread() override {
    web_app::WebAppNavigationBrowserTest::SetUpOnMainThread();
    InstallTestWebApp();
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallWebApp(profile(), test_web_app_id());
    if (!overlapping_app_id_.empty()) {
      web_app::test::UninstallWebApp(profile(), overlapping_app_id_);
    }
    web_app::WebAppNavigationBrowserTest::TearDownOnMainThread();
  }

  template <typename Action>
  void DoAndWaitForIntentPickerIconUpdate(Action action) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    base::RunLoop run_loop;
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(web_contents);
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    action();
    run_loop.Run();
  }

  void OpenNewTab(const GURL& url) {
    chrome::NewTab(browser());
    DoAndWaitForIntentPickerIconUpdate(
        [this] { NavigateToLaunchingPage(browser()); });
    ClickLinkAndWaitForIconUpdate(
        browser()->tab_strip_model()->GetActiveWebContents(), url);
  }

  IntentChipButton* GetIntentChip() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetIntentChipButton();
  }

  // Clicks the intent chip, and optionally waits for a browser app window to
  // appear if `wait_for_browser` is true. If waiting is specified, the new
  // browser window is returned; if waiting is not specified, null is returned.
  Browser* ClickIntentChip(bool wait_for_browser) {
    ui_test_utils::BrowserChangeObserver browser_opened(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

    views::test::ButtonTestApi test_api(GetIntentChip());
    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    test_api.NotifyClick(e);

    if (wait_for_browser) {
      return browser_opened.Wait();
    }

    return nullptr;
  }

  void ClickLinkAndWaitForIconUpdate(content::WebContents* web_contents,
                                     const GURL& link_url) {
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(web_contents);

    base::RunLoop run_loop;
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    ClickLinkAndWait(web_contents, link_url, LinkTarget::SELF, "");
    run_loop.Run();
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
    DCHECK(!overlapping_app_id_.empty());
    web_app::AppReadinessWaiter(profile(), overlapping_app_id_).Await();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::AppId overlapping_app_id_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       NavigationToInScopeLinkShowsIntentChip) {
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));

  EXPECT_TRUE(GetIntentChip()->GetVisible());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  ClickIntentChip(/*wait_for_browser=*/false);

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(IntentPickerBubbleView::intent_picker_bubble());
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       NavigationToOutOfScopeLinkDoesNotShowsIntentChip) {
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), out_of_scope_url));

  EXPECT_FALSE(GetIntentChip()->GetVisible());
}

// TODO(crbug.com/1395393): This test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_IconVisibilityAfterTabSwitching \
  DISABLED_IconVisibilityAfterTabSwitching
#else
#define MAYBE_IconVisibilityAfterTabSwitching IconVisibilityAfterTabSwitching
#endif
IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       MAYBE_IconVisibilityAfterTabSwitching) {
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

#if BUILDFLAG(IS_CHROMEOS)
// Using the Intent Chip for an app which is set as preferred should launch
// directly into the app. Preferred apps are only available on ChromeOS.
IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest, OpensAppForPreferredApp) {
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), test_web_app_id());

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));

  Browser* app_browser = ClickIntentChip(/*wait_for_browser=*/true);

  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       ShowsIntentChipExpandedForPreferredApp) {
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), test_web_app_id());

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  // First three visits will always show as expanded.
  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
    EXPECT_TRUE(GetIntentChip()->GetVisible());
    EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), out_of_scope_url));
    EXPECT_FALSE(GetIntentChip()->GetVisible());
  }

  // Fourth visit should show as expanded because the app is set as preferred
  // for this URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class IntentChipButtonSkipIntentPickerBrowserTest
    : public IntentChipButtonBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      apps::features::kIntentChipSkipsPicker};
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonSkipIntentPickerBrowserTest,
                       ClickingChipOpensApp) {
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));

  Browser* app_browser = ClickIntentChip(/*wait_for_browser=*/true);

  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}

// TODO(crbug.com/1313274): Fix test flakiness on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ShowsIntentPickerWhenMultipleApps \
  DISABLED_ShowsIntentPickerWhenMultipleApps
#else
#define MAYBE_ShowsIntentPickerWhenMultipleApps \
  ShowsIntentPickerWhenMultipleApps
#endif
IN_PROC_BROWSER_TEST_F(IntentChipButtonSkipIntentPickerBrowserTest,
                       MAYBE_ShowsIntentPickerWhenMultipleApps) {
  InstallOverlappingApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
  base::RunLoop().RunUntilIdle();

  // The Intent Chip should appear, but the intent picker bubble should not
  // appear automatically.
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(IntentPickerBubbleView::intent_picker_bubble());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  ClickIntentChip(/*wait_for_browser=*/false);

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(IntentPickerBubbleView::intent_picker_bubble());
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonSkipIntentPickerBrowserTest,
                       ShowsIntentChipCollapsed) {
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
  ClickLinkAndWaitForIconUpdate(web_contents, in_scope_url);
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

  ClickLinkAndWaitForIconUpdate(web_contents, separate_host_url);
  EXPECT_FALSE(GetIntentChip()->GetVisible());

  // 2nd appearance: Expanded.
  ClickLinkAndWaitForIconUpdate(web_contents, in_scope_url);
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

  ClickLinkAndWaitForIconUpdate(web_contents, out_of_scope_url);
  EXPECT_FALSE(GetIntentChip()->GetVisible());

  // 3rd appearance: Expanded.
  ClickLinkAndWaitForIconUpdate(web_contents, in_scope_url);
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

  ClickLinkAndWaitForIconUpdate(web_contents, out_of_scope_url);
  EXPECT_FALSE(GetIntentChip()->GetVisible());

  // 4th appearance: Collapsed.
  ClickLinkAndWaitForIconUpdate(web_contents, in_scope_url);
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_TRUE(GetIntentChip()->is_fully_collapsed());

  // Click to open app and reset the counter.
  ClickIntentChip(/*wait_for_browser=*/true);

  // Open another browser- we should be able to see the expanded chip again.
  DoAndWaitForIntentPickerIconUpdate(
      [this] { NavigateToLaunchingPage(browser()); });

  // 1st appearance since intent chip counter reset: Expanded.
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ClickLinkAndWaitForIconUpdate(web_contents, in_scope_url);
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

// TODO(crbug.com/1393003): This test is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(IntentChipButtonIPHBubbleBrowserTest,
                       DISABLED_ShowAndCloseIPH) {
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
      browser_view->GetFeaturePromoController()));

  NavigateToLaunchingPage(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to an in-scope page to see the intent chip and the IPH.
  ClickLinkAndWaitForIconUpdate(web_contents, in_scope_url);
  EXPECT_TRUE(GetIntentChip()->GetVisible());

  // Wait for the chip to actually be laid out. This will result in the IPH
  // showing.
  base::RunLoop chip_loop;
  NonzeroSizeObserver observer(GetIntentChip(), chip_loop.QuitClosure());
  chip_loop.Run();

  // Check if the IPH bubble is showing.
  EXPECT_TRUE(browser_view->IsFeaturePromoActive(
      feature_engagement::kIPHIntentChipFeature));

  // When we click on the intent chip, the IPH should disappear.
  ClickIntentChip(/*wait_for_browser=*/false);

  // Check the IPH is no longer showing.
  EXPECT_FALSE(browser_view->IsFeaturePromoActive(
      feature_engagement::kIPHIntentChipFeature));
}

class IntentChipButtonAppIconBrowserTest : public IntentChipButtonBrowserTest {
 public:
  IntentChipButtonAppIconBrowserTest() {
    feature_list_.InitAndEnableFeature(apps::features::kIntentChipAppIcon);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonAppIconBrowserTest, ShowsAppIconInChip) {
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

#if BUILDFLAG(IS_CHROMEOS)
// Test fixture class which shows the supported links infobar when opening an
// app through the intent picker.
class IntentChipWithInfoBarBrowserTest : public IntentChipButtonBrowserTest {
 public:
  IntentChipWithInfoBarBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{apps::features::kIntentChipSkipsPicker,
                              apps::features::kLinkCapturingInfoBar},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IntentChipWithInfoBarBrowserTest,
                       ShowsInfoBarOnAppOpen) {
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  DoAndWaitForIntentPickerIconUpdate([this, in_scope_url] {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
  });
  EXPECT_TRUE(GetIntentChip()->GetVisible());

  Browser* app_browser = ClickIntentChip(/*wait_for_browser=*/true);

  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      app_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(infobar_manager->infobar_count(), 1u);
  ASSERT_EQ(
      infobar_manager->infobar_at(0)->delegate()->GetIdentifier(),
      infobars::InfoBarDelegate::SUPPORTED_LINKS_INFOBAR_DELEGATE_CHROMEOS);
}

// Test fixture class which automatically displays the intent picker bubble when
// a link is clicked to a page with an installed app.
class IntentChipWithAutoDisplayBrowserTest
    : public IntentChipButtonBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      apps::features::kLinkCapturingAutoDisplayIntentPicker};
};

IN_PROC_BROWSER_TEST_F(IntentChipWithAutoDisplayBrowserTest,
                       ShowsIntentPickerOnNavigation) {
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  NavigateToLaunchingPage(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       IntentPickerBubbleView::kViewClassName);

  ClickLinkAndWait(web_contents, in_scope_url, LinkTarget::SELF, "");

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(IntentPickerBubbleView::intent_picker_bubble());
}
#endif  // BUILDFLAG(IS_CHROMEOS)
