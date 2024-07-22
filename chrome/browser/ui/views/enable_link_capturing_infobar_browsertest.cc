// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/ranges/algorithm.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {
namespace {

content::WebContents* GetActiveWebContents(Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

class EnableLinkCapturingInfobarBrowserTest
    : public WebAppNavigationBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  EnableLinkCapturingInfobarBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(
            /*override_captures_by_default=*/GetParam()),
        {});
  }

  bool LinkCapturingEnabledByDefault() { return GetParam(); }

  // Returns [app_id, in_scope_url]
  std::tuple<webapps::AppId, GURL> InstallTestApp() {
    GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
    GURL in_scope_url = embedded_test_server()->GetURL("/web_apps/page1.html");

    webapps::AppId app_id =
        InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url);
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return {app_id, in_scope_url};
  }

  // Returns [outer app_id, inner app_id, inner app in_scope_url]
  std::tuple<webapps::AppId, webapps::AppId, GURL>
  InstallOuterAppAndInnerApp() {
    GURL outer_start_url =
        embedded_test_server()->GetURL("/web_apps/nesting/index.html");
    GURL inner_start_url =
        embedded_test_server()->GetURL("/web_apps/nesting/nested/index.html");
    GURL inner_in_scope_url =
        embedded_test_server()->GetURL("/web_apps/nesting/nested/page1.html");

    // The inner app must be installed first so that it is installable.
    webapps::AppId inner_app_id =
        InstallWebAppFromPageAndCloseAppBrowser(browser(), inner_start_url);
    apps::AppReadinessWaiter(profile(), inner_app_id).Await();
    webapps::AppId outer_app_id =
        InstallWebAppFromPageAndCloseAppBrowser(browser(), outer_start_url);
    apps::AppReadinessWaiter(profile(), outer_app_id).Await();
    return {outer_app_id, inner_app_id, inner_in_scope_url};
  }

  void NavigateViaLinkClick(Browser* browser,
                            const GURL& url,
                            LinkTarget link_target = LinkTarget::SELF) {
    ClickLinkAndWait(GetActiveWebContents(browser), url, link_target,
                     std::string());
  }

  infobars::InfoBar* GetLinkCapturingInfoBar(Browser* browser) {
    return GetLinkCapturingInfoBar(GetActiveWebContents(browser));
  }

  infobars::InfoBar* GetLinkCapturingInfoBar(
      content::WebContents* web_contents) {
    return apps::EnableLinkCapturingInfoBarDelegate::FindInfoBar(web_contents);
  }

  void TurnOnLinkCapturing(webapps::AppId app_id) {
    base::test::TestFuture<void> preference_set;
    provider().scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app_id, /*set_to_preferred=*/true, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

  void TurnOffLinkCapturing(webapps::AppId app_id) {
    base::test::TestFuture<void> preference_set;
    provider().scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app_id, /*set_to_preferred=*/false, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest,
                       InfoBarShowsOnIntentPickerLaunch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(app_id);
  }

  NavigateViaLinkClick(browser(), in_scope_url);

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);

  EXPECT_NE(GetLinkCapturingInfoBar(app_browser), nullptr);
}

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest,
                       EnableLinkCapturingThroughInfoBar) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(app_id);
  }

  NavigateViaLinkClick(browser(), in_scope_url);

  Browser* app_browser;
  {
    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
    app_browser = browser_added_waiter.Wait();
    ASSERT_TRUE(app_browser);
  }
  infobars::InfoBar* infobar = GetLinkCapturingInfoBar(app_browser);
  ASSERT_TRUE(infobar);

  base::UserActionTester user_action_tester;
  // Because there is no testing utility for info bars, manually accept &
  // remove.
  EXPECT_TRUE(
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->Accept());
  infobar->RemoveSelf();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingAcceptedFromInfoBar"));

  EXPECT_EQ(app_id,
            provider().registrar_unsafe().FindAppThatCapturesLinksInScope(
                in_scope_url));
}

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest,
                       InfoBarNotShownOnLinkCapturingEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (!LinkCapturingEnabledByDefault()) {
    TurnOnLinkCapturing(app_id);
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  // If there is only 1 app installed that captures the document URL, and that
  // app is also set to capture links by default, the link should open in the
  // PWA automatically on clicking the intent chip without going through the
  // intent picker bubble.
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);

  EXPECT_EQ(GetLinkCapturingInfoBar(app_browser), nullptr);
}

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest,
                       RecordUserActionCancelled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(app_id);
  }

  NavigateViaLinkClick(browser(), in_scope_url);

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);

  infobars::InfoBar* infobar = GetLinkCapturingInfoBar(app_browser);
  ASSERT_TRUE(infobar);

  base::UserActionTester user_action_tester;
  // Because there is no testing utility for info bars, manually cancel &
  // remove.
  EXPECT_TRUE(
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->Cancel());
  infobar->RemoveSelf();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "LinkCapturingCancelledFromInfoBar"));

  EXPECT_EQ(std::nullopt,
            provider().registrar_unsafe().FindAppThatCapturesLinksInScope(
                in_scope_url));
}

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest,
                       RecordUserActionIgnored) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(app_id);
  }

  NavigateViaLinkClick(browser(), in_scope_url);

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);

  infobars::InfoBar* infobar = GetLinkCapturingInfoBar(app_browser);
  ASSERT_TRUE(infobar);

  base::UserActionTester user_action_tester;

  CloseBrowserSynchronously(app_browser);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("LinkCapturingIgnoredFromInfoBar"));

  EXPECT_EQ(std::nullopt,
            provider().registrar_unsafe().FindAppThatCapturesLinksInScope(
                in_scope_url));
}

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest, AppLaunched) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(app_id);
  }

  NavigateViaLinkClick(browser(), in_scope_url);

  Browser* app_browser;
  {
    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
    app_browser = browser_added_waiter.Wait();
    ASSERT_TRUE(app_browser);
  }

  infobars::InfoBar* infobar = GetLinkCapturingInfoBar(app_browser);
  ASSERT_TRUE(infobar);
  EXPECT_TRUE(
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->Accept());
  // Because there is no testing utility for info bars, manually remove.
  infobar->RemoveSelf();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  CloseBrowserSynchronously(app_browser);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  content::TestNavigationObserver observer =
      content::TestNavigationObserver(GURL(url::kAboutBlankURL));
  observer.StartWatchingNewWebContents();
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/-1,
                   /*foreground=*/true);
  observer.Wait();
  {
    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    NavigateViaLinkClick(browser(), in_scope_url);
    app_browser = browser_added_waiter.Wait();
    ASSERT_TRUE(app_browser);
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  }
}

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest, BarRemoved) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(app_id);
  }

  NavigateViaLinkClick(browser(), in_scope_url);

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);

  // The web_contents here is moving to `browser()`, and `app_browser` will be
  // invalidated, so grab a WeakPtr here.
  base::WeakPtr<content::WebContents> web_contents =
      GetActiveWebContents(app_browser)->GetWeakPtr();

  EXPECT_TRUE(GetLinkCapturingInfoBar(web_contents.get()));

  // Note: this will close & invalidate app_browser.
  Browser* tabbed_browser = chrome::OpenInChrome(app_browser);

  ASSERT_TRUE(web_contents);
  EXPECT_EQ(web_contents.get(), GetActiveWebContents(tabbed_browser));
  EXPECT_FALSE(GetLinkCapturingInfoBar(web_contents.get()));
}

// TODO(crbug.com/40936015): Flaky on all platforms.
IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest,
                       DISABLED_InfoBarHiddenAfterDismissals) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [app_id, in_scope_url] = InstallTestApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(app_id);
  }

  // Dismiss the infobar twice.
  for (int i = 0; i < 2; ++i) {
    NavigateViaLinkClick(browser(), in_scope_url);

    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
    Browser* app_browser = browser_added_waiter.Wait();
    ASSERT_TRUE(app_browser);

    infobars::InfoBar* infobar = GetLinkCapturingInfoBar(app_browser);
    ASSERT_TRUE(infobar);

    base::UserActionTester user_action_tester;
    EXPECT_TRUE(
        static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->Cancel());
    // Because there is no testing utility for info bars, manually remove.
    infobar->RemoveSelf();
    provider().command_manager().AwaitAllCommandsCompleteForTesting();

    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "LinkCapturingCancelledFromInfoBar"));

    EXPECT_EQ(std::nullopt,
              provider().registrar_unsafe().FindAppThatCapturesLinksInScope(
                  in_scope_url));
    CloseBrowserSynchronously(app_browser);
  }

  // Now, the infobar will not show up.
  NavigateViaLinkClick(browser(), in_scope_url);

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);

  infobars::InfoBar* infobar = GetLinkCapturingInfoBar(app_browser);
  EXPECT_FALSE(infobar);
}

IN_PROC_BROWSER_TEST_P(EnableLinkCapturingInfobarBrowserTest,
                       OuterAppNoInfoBar) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto [outer_app_id, inner_app_id, inner_app_scoped_url] =
      InstallOuterAppAndInnerApp();

  // The infobar shows up only when link capturing is not enabled for an app.
  // For default-on behavior, switch off link capturing manually.
  if (LinkCapturingEnabledByDefault()) {
    TurnOffLinkCapturing(outer_app_id);
    TurnOffLinkCapturing(inner_app_id);
  }

  NavigateViaLinkClick(browser(), inner_app_scoped_url);
  EXPECT_TRUE(web_app::ClickIntentPickerAndWaitForBubble(browser()));

  // The app list is currently not deterministically ordered, so find the
  // correct item and select that.
  const auto& app_infos =
      web_app::intent_picker_bubble()->app_info_for_testing();
  auto it = base::ranges::find(app_infos, outer_app_id,
                               &apps::IntentPickerAppInfo::launch_name);
  ASSERT_NE(it, app_infos.end());
  size_t index = it - app_infos.begin();

  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  views::test::ButtonTestApi test_api(
      web_app::GetIntentPickerButtonAtIndex(index));
  test_api.NotifyClick(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  web_app::intent_picker_bubble()->AcceptDialog();

  Browser* app_browser = browser_added_waiter.Wait();
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, outer_app_id));
  EXPECT_FALSE(GetLinkCapturingInfoBar(app_browser));
}

INSTANTIATE_TEST_SUITE_P(,
                         EnableLinkCapturingInfobarBrowserTest,
                         testing::Values(true, false),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "DefaultOn" : "DefaultOff";
                         });

}  // namespace
}  // namespace web_app
