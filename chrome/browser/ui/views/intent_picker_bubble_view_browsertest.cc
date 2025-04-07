// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button_test_base.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/view_utils.h"
#endif

using LinkCapturingFeatureVersion = apps::test::LinkCapturingFeatureVersion;

namespace {

std::string GetLinkCapturingTestName(
    const testing::TestParamInfo<
        std::tuple<std::string, LinkCapturingFeatureVersion, bool>>& info) {
  std::string test_name;
  test_name = std::get<std::string>(info.param);
  test_name.append("_");
  test_name.append(
      apps::test::ToString(std::get<LinkCapturingFeatureVersion>(info.param)));
  test_name.append(std::get<bool>(info.param) ? "MigrationEnabled"
                                              : "MigrationNotEnabled");
  return test_name;
}

}  // namespace

class IntentPickerBrowserTest : public web_app::WebAppNavigationBrowserTest {
 public:
  IntentPickerBrowserTest() {
    if (IsMigrationEnabled()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kPageActionsMigration,
          {{features::kPageActionsMigrationIntentPicker.name, "true"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {::features::kPageActionsMigration});
    }
  }

  template <typename Action>
  testing::AssertionResult DoAndWaitForIntentPickerIconUpdate(Action action) {
    base::test::TestFuture<void> intent_picker_done;
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
    tab_helper->SetIconUpdateCallbackForTesting(
        intent_picker_done.GetCallback());
    // On Mac, updating the icon requires asynchronous work that is done on the
    // threadpool (see `WebAppsIntentPickerDelegate::FindAllAppsForUrl()` for
    // more information). Flushing the thread pool thus helps prevent flakiness
    // in tests.
#if BUILDFLAG(IS_MAC)
    base::ThreadPoolInstance::Get()->FlushForTesting();
#endif  // BUILDFLAG(IS_MAC)
    action();
    if (HasFailure()) {
      return testing::AssertionFailure();
    }
    if (intent_picker_done.Wait()) {
      return testing::AssertionSuccess();
    }
    return testing::AssertionFailure() << "Intent picker never resolved";
  }

  content::WebContents* OpenNewTab(const GURL& url,
                                   const std::string& rel = "") {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    EXPECT_TRUE(DoAndWaitForIntentPickerIconUpdate(
        [this] { NavigateToLaunchingPage(browser()); }));
    EXPECT_TRUE(DoAndWaitForIntentPickerIconUpdate([this, url] {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    }));

    return web_contents;
  }

  // Inserts an iframe in the main frame of |web_contents|.
  bool InsertIFrame(content::WebContents* web_contents) {
    return content::ExecJs(web_contents,
                           "let iframe = document.createElement('iframe');"
                           "iframe.id = 'iframe';"
                           "document.body.appendChild(iframe);");
  }


  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }
  // Returns a bool indicating whether the ongoing page action framework
  // migration is enabled. This function provides a default implementation that
  // can be overridden in tests to control the enabled state of the page action
  // view.
  virtual bool IsMigrationEnabled() const { return false; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests to do with the behavior of the intent picker icon in the omnibox. Does
// not test the behavior of the intent picker bubble itself.
// Note that behavior specific to the chip version of the icon is tested
// separately in intent_chip_button_browsertest.cc.
class IntentPickerIconBrowserTest
    : public IntentPickerBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, LinkCapturingFeatureVersion, bool>>,
      public IntentChipButtonTestBase {
 public:
  // TODO(crbug.com/40097608): Stop disabling Paint Holding.
  IntentPickerIconBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> features_to_enable =
        apps::test::GetFeaturesToEnableLinkCapturingUX(LinkCapturingVersion());

    features_to_enable.push_back({blink::features::kPaintHolding, {}});
    if (IsMigrationEnabled()) {
      features_to_enable.push_back(
          {::features::kPageActionsMigration,
           {{::features::kPageActionsMigrationIntentPicker.name, "true"}}});
    }

    feature_list_.InitWithFeaturesAndParameters(features_to_enable, {});
  }

  bool IsMigrationEnabled() const override {
    return std::get<bool>(GetParam());
  }

  LinkCapturingFeatureVersion LinkCapturingVersion() {
    return std::get<LinkCapturingFeatureVersion>(GetParam());
  }

  std::string rel() { return std::get<std::string>(GetParam()); }

  bool IsDefaultOnEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
    return false;
#else
    return LinkCapturingVersion() == LinkCapturingFeatureVersion::kV2DefaultOn;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that clicking a link from a tabbed browser to outside the scope of an
// installed app does not show the intent picker.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       NavigationToOutOfScopeLinkDoesNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateToLaunchingPage(browser());
  ASSERT_TRUE(ExpectLinkClickNotCapturedIntoAppBrowser(
      browser(), out_of_scope_url, rel()));

  EXPECT_FALSE(GetIntentChip(browser())->GetVisible());

  EXPECT_EQ(nullptr, intent_picker_bubble());
}

// Tests that clicking a link from a tabbed browser to within the scope of an
// installed app shows the intent picker icon in Omnibox.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       NavigationToInScopeLinkShowsIntentPicker) {
  if (IsDefaultOnEnabled()) {
    GTEST_SKIP() << "Default On will launch app by default";
  }
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
  NavigateToLaunchingPage(browser());

  base::RunLoop run_loop;
  tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
  ASSERT_TRUE(
      ExpectLinkClickNotCapturedIntoAppBrowser(browser(), in_scope_url, rel()));
  run_loop.Run();

  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(GetIntentChip(browser())->GetVisible());
}

// Tests that the intent icon updates its visibility when switching between
// tabs.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  // OpenNewTab opens a new tab and focus on the new tab.
  OpenNewTab(in_scope_url, /*rel=*/rel());
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(GetIntentChip(browser())->GetVisible());
  OpenNewTab(out_of_scope_url, /*rel=*/rel());
  EXPECT_FALSE(GetIntentChip(browser())->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(GetIntentChip(browser())->GetVisible());
  chrome::SelectNextTab(browser());
  EXPECT_FALSE(GetIntentChip(browser())->GetVisible());
}

// Tests that the navigation in iframe doesn't affect intent picker icon
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       IframeNavigationDoesNotAffectIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  views::Button* intent_picker_icon = GetIntentChip(browser());

  content::WebContents* initial_tab = OpenNewTab(out_of_scope_url);
  ASSERT_TRUE(InsertIFrame(initial_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", in_scope_url));
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  content::WebContents* new_tab = OpenNewTab(in_scope_url);
  ASSERT_TRUE(InsertIFrame(new_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", out_of_scope_url));
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(intent_picker_icon->GetVisible());
}

// Tests that the intent picker icon is not visible if the navigation redirects
// to a URL that doesn't have an installed PWA.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       DoesNotShowIntentPickerWhenRedirectedOutOfScope) {
  InstallTestWebApp(GetOtherAppUrlHost(), /*app_scope=*/"/");

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  const GURL in_scope_url = https_server().GetURL(GetOtherAppUrlHost(), "/");
  const GURL redirect_url = https_server().GetURL(
      GetOtherAppUrlHost(), CreateServerRedirect(out_of_scope_url));

  views::Button* intent_picker_icon = GetIntentChip(browser());

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(intent_picker_icon->GetVisible());
  ASSERT_TRUE(DoAndWaitForIntentPickerIconUpdate(
      [this, redirect_url, out_of_scope_url] {
        ClickLinkAndWaitForURL(GetWebContents(), redirect_url, out_of_scope_url,
                               LinkTarget::SELF, rel());
      }));
  EXPECT_FALSE(intent_picker_icon->GetVisible());
}

// Test that navigating to service pages (chrome://) will hide the intent picker
// icon.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       DoNotShowIconAndBubbleOnServicePages) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  GURL chrome_pages_url("chrome://version");
  std::string app_name = "test_name";

  views::Button* intent_picker_view = GetIntentChip(browser());

  OpenNewTab(in_scope_url);
  ASSERT_TRUE(intent_picker_view);
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(intent_picker_view->GetVisible());

  // Now switch to chrome://version.
  ASSERT_TRUE(DoAndWaitForIntentPickerIconUpdate([this, &chrome_pages_url]() {
    NavigateParams params(browser(), chrome_pages_url,
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    // Navigates and waits for loading to finish.
    ui_test_utils::NavigateToURL(&params);
  }));

  // Make sure that the intent picker icon is no longer visible.
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Test that error pages do not show the intent picker icon.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest, DoNotShowIconOnErrorPages) {
  InstallTestWebApp();
  InstallTestWebApp("www.google.com", "/");

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  views::Button* intent_picker_view = GetIntentChip(browser());
  ASSERT_TRUE(intent_picker_view);

  // Go to the test app and wait for the intent picker icon to load.
  OpenNewTab(in_scope_url);
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(intent_picker_view->GetVisible());

  // Now switch to www.google.com, which gives a network error in the test
  // environment.
  ASSERT_TRUE(DoAndWaitForIntentPickerIconUpdate([this]() {
    NavigateParams params(browser(), GURL("https://www.google.com"),
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    // Navigates and waits for loading to finish.
    ui_test_utils::NavigateToURL(&params);
  }));

  // Make sure that the intent picker icon is not shown on the error page, even
  // though there's a PWA available for www.google.com.
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Test that loading a page with pushState() call that changes URL updates the
// intent picker view.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest, PushStateURLChangeTest) {
  // Note: The test page is served from embedded_test_server() as https_server()
  // always returns empty responses.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server().Start());

  const GURL test_url =
      embedded_test_server()->GetURL("/intent_picker/push_state_test.html");
  web_app::test::InstallDummyWebApp(profile(), "Test app", test_url);

  views::Button* intent_picker_view = GetIntentChip(browser());

  OpenNewTab(test_url);
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(intent_picker_view->GetVisible());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(DoAndWaitForIntentPickerIconUpdate([web_contents] {
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.getElementById('push_to_new_url_button').click();"));
  }));

  EXPECT_FALSE(intent_picker_view->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerIconBrowserTest,
    testing::Combine(testing::Values("", "noopener", "noreferrer", "nofollow"),
#if BUILDFLAG(IS_CHROMEOS)
                     testing::Values(LinkCapturingFeatureVersion::kV1DefaultOff,
                                     LinkCapturingFeatureVersion::kV2DefaultOff)
#else
                     testing::Values(LinkCapturingFeatureVersion::kV2DefaultOn,
                                     LinkCapturingFeatureVersion::kV2DefaultOff)
#endif  // BUILDFLAG(IS_CHROMEOS)
                         ,
                     testing::Bool()),
    GetLinkCapturingTestName);

class IntentPickerIconBrowserBubbleTest
    : public IntentPickerBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, LinkCapturingFeatureVersion, bool>>,
      public IntentChipButtonTestBase {
 public:
  // TODO(crbug.com/40097608): Stop disabling Paint Holding.
  IntentPickerIconBrowserBubbleTest() {
    std::vector<base::test::FeatureRefAndParams> features_to_enable =
        apps::test::GetFeaturesToEnableLinkCapturingUX(LinkCapturingVersion());

    if (IsMigrationEnabled()) {
      features_to_enable.push_back(
          {::features::kPageActionsMigration,
           {{::features::kPageActionsMigrationIntentPicker.name, "true"}}});
    }

    feature_list_.InitWithFeaturesAndParameters(
        features_to_enable, {blink::features::kPaintHolding});
  }

  bool IsMigrationEnabled() const override {
    return std::get<bool>(GetParam());
  }

  LinkCapturingFeatureVersion LinkCapturingVersion() const {
    return std::get<LinkCapturingFeatureVersion>(GetParam());
  }
  bool LinkCapturingEnabledByDefault() const {
#if BUILDFLAG(IS_CHROMEOS)
    return false;
#else
    return LinkCapturingVersion() == LinkCapturingFeatureVersion::kV2DefaultOn;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  size_t GetItemContainerSize(IntentPickerBubbleView* bubble) {
    return bubble->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
        ->children()
        .size();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserBubbleTest,
                       IntentChipOpensBubble) {
  InstallTestWebApp();
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  OpenNewTab(in_scope_url);
  ASSERT_TRUE(web_app::ClickIntentPickerAndWaitForBubble(browser()));

  EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(test_web_app_id(), app_info[0].launch_name);
  EXPECT_EQ(GetAppName(), app_info[0].display_name);
}

// Test that the "Remember this choice" checkbox works.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserBubbleTest, RememberOpenWebApp) {
  base::HistogramTester histogram_tester;

  InstallTestWebApp();
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  OpenNewTab(in_scope_url);
  ASSERT_TRUE(web_app::ClickIntentPickerAndWaitForBubble(browser()));

  // Check "Remember my choice" and accept the bubble.
  views::Checkbox* remember_selection_checkbox =
      views::AsViewClass<views::Checkbox>(intent_picker_bubble()->GetViewByID(
          IntentPickerBubbleView::ViewId::kRememberCheckbox));
  ASSERT_TRUE(remember_selection_checkbox);
  ASSERT_TRUE(remember_selection_checkbox->GetEnabled());
  remember_selection_checkbox->SetChecked(true);

  apps::PreferredAppsListHandle& preferred_apps =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->PreferredAppsList();
  apps_util::PreferredAppUpdateWaiter preference_update_waiter(
      preferred_apps, test_web_app_id());

  ui_test_utils::BrowserChangeObserver added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  intent_picker_bubble()->AcceptDialog();

  // Accepting the bubble should open the app.
  Browser* app_browser = added_observer.Wait();
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));

  // The link capturing preference should be updated.
  preference_update_waiter.Wait();
  ASSERT_TRUE(
      preferred_apps.IsPreferredAppForSupportedLinks(test_web_app_id()));

  // Check that we recorded that settings were changed.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged, 0);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged, 1);
}

#else
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserBubbleTest,
                       DISABLED_IntentChipLaunchesAppDirectly) {
  InstallTestWebApp();
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  views::Button* intent_picker_icon = GetIntentChip(browser());

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_icon->GetVisible());

  views::test::ButtonTestApi test_api(intent_picker_icon);
  test_api.NotifyClick(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  Browser* app_browser = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_FALSE(intent_picker_bubble());
  EXPECT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerIconBrowserBubbleTest,
    testing::Combine(testing::Values("", "noopener", "noreferrer", "nofollow"),
#if BUILDFLAG(IS_CHROMEOS)
                     testing::Values(LinkCapturingFeatureVersion::kV1DefaultOff,
                                     LinkCapturingFeatureVersion::kV2DefaultOff)
#else
                     testing::Values(LinkCapturingFeatureVersion::kV2DefaultOn,
                                     LinkCapturingFeatureVersion::kV2DefaultOff)
#endif  // BUILDFLAG(IS_CHROMEOS)
                         ,
                     testing::Bool()),
    GetLinkCapturingTestName);

// This test only works when link capturing is set to default off for desktop
// platforms, as prerendering navigations are aborted during link captured app
// launches. See LinkCapturingNavigationThrottle::MaybeCreate for more
// information.
// TODO(b/297256243): Investigate prerendering integration with link capturing.
class IntentPickerIconPrerenderingBrowserTest
    : public IntentPickerIconBrowserTest {
 public:
  IntentPickerIconPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &IntentPickerIconPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~IntentPickerIconPrerenderingBrowserTest() override = default;
  IntentPickerIconPrerenderingBrowserTest(
      const IntentPickerIconPrerenderingBrowserTest&) = delete;

  IntentPickerIconPrerenderingBrowserTest& operator=(
      const IntentPickerIconPrerenderingBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    IntentPickerIconBrowserTest::SetUp();
  }

  bool IsMigrationEnabled() const override {
    return std::get<bool>(GetParam());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    IntentPickerIconBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_P(IntentPickerIconPrerenderingBrowserTest,
                       PrerenderingShouldNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL initial_url =
      https_server().GetURL(GetAppUrlHost(), "/empty.html");
  OpenNewTab(initial_url);

  views::Button* intent_picker_icon = GetIntentChip(browser());
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  // Load a prerender page and prerendering should not try to show the
  // intent picker.
  const GURL prerender_url = https_server().GetURL(
      GetAppUrlHost(), std::string(GetAppScopePath()) + "index1.html");
  content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  // Activate the prerender page.
  ASSERT_TRUE(DoAndWaitForIntentPickerIconUpdate([this, prerender_url] {
    prerender_test_helper().NavigatePrimaryPage(prerender_url);
  }));
  EXPECT_TRUE(host_observer.was_activated());

  // After activation, IntentPickerTabHelper should show the
  // intent picker.
  EXPECT_TRUE(
      WaitForPageActionButtonVisible(kActionShowIntentPicker, browser()));
  EXPECT_TRUE(intent_picker_icon->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerIconPrerenderingBrowserTest,
    testing::Combine(testing::Values("", "noopener", "noreferrer", "nofollow"),
#if BUILDFLAG(IS_CHROMEOS)
                     testing::Values(LinkCapturingFeatureVersion::kV1DefaultOff,
                                     LinkCapturingFeatureVersion::kV2DefaultOff)
#else
                     testing::Values(LinkCapturingFeatureVersion::kV2DefaultOff)
#endif  // BUILDFLAG(IS_CHROMEOS)
                         ,
                     testing::Bool()),
    GetLinkCapturingTestName);

class IntentPickerIconFencedFrameBrowserTest
    : public IntentPickerIconBrowserTest {
 public:
  IntentPickerIconFencedFrameBrowserTest() = default;
  ~IntentPickerIconFencedFrameBrowserTest() override = default;
  IntentPickerIconFencedFrameBrowserTest(
      const IntentPickerIconFencedFrameBrowserTest&) = delete;

  IntentPickerIconFencedFrameBrowserTest& operator=(
      const IntentPickerIconFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  bool IsMigrationEnabled() const override {
    return std::get<bool>(GetParam());
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_P(IntentPickerIconFencedFrameBrowserTest,
                       ShouldShowIntentPickerInFencedFrame) {
  InstallTestWebApp();

  views::Button* intent_picker_icon = GetIntentChip(browser());

  const GURL initial_url =
      https_server().GetURL(GetAppUrlHost(), "/empty.html");
  OpenNewTab(initial_url);
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  const GURL fenced_frame_url = https_server().GetURL(
      GetAppUrlHost(), std::string(GetAppScopePath()) + "index1.html");
  // Create a fenced frame.
  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url));

  EXPECT_FALSE(intent_picker_icon->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerIconFencedFrameBrowserTest,
    testing::Combine(testing::Values("", "noopener", "noreferrer", "nofollow"),
#if BUILDFLAG(IS_CHROMEOS)
                     testing::Values(LinkCapturingFeatureVersion::kV1DefaultOff,
                                     LinkCapturingFeatureVersion::kV2DefaultOff)
#else
                     testing::Values(LinkCapturingFeatureVersion::kV2DefaultOn,
                                     LinkCapturingFeatureVersion::kV2DefaultOff)
#endif  // BUILDFLAG(IS_CHROMEOS)
                         ,
                     testing::Bool()),
    GetLinkCapturingTestName);
