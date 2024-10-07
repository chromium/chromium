// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/navigation_capturing_information_forwarder.h"
#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

namespace {

constexpr char kStartPageScopeA[] =
    "/banners/link_capturing/scope_a/start.html";
constexpr char kDestinationPageScopeB[] =
    "/banners/link_capturing/scope_b/destination.html";
constexpr char kToSiteATargetBlankWithOpener[] = "id-LINK-A_TO_A-BLANK-OPENER";
constexpr char kToSiteBTargetBlankNoopener[] = "id-LINK-A_TO_B-BLANK-NO_OPENER";
constexpr char kToSiteBTargetBlankWithOpener[] = "id-LINK-A_TO_B-BLANK-OPENER";

class NavigationCompletionAwaiter : public content::WebContentsObserver,
                                    public ui_test_utils::AllTabsObserver {
 public:
  explicit NavigationCompletionAwaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {
    AddAllBrowsers();
  }
  ~NavigationCompletionAwaiter() override = default;

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    auto* navigation_handle_data =
        web_app::NavigationCapturingNavigationHandleUserData::
            GetForNavigationHandle(*handle);
    if (navigation_handle_data) {
      redirection_info_ = navigation_handle_data->redirection_info();
    }
    ConditionMet();
  }

  void AwaitNavigationCompletion() { Wait(); }

  std::optional<NavigationCapturingRedirectionInfo>
  GetRedirectionInfoForNavigation() {
    return redirection_info_;
  }

  // AllTabsObserver override:
  std::unique_ptr<base::CheckedObserver> ProcessOneContents(
      content::WebContents* web_contents) override {
    // Stop observing the current `WebContents` if a new one has been created,
    // so that we can process the `WebContents` navigation completed in.
    if (content::WebContentsObserver::IsInObserverList()) {
      Observe(nullptr);
    }
    Observe(web_contents);
    return nullptr;
  }

 private:
  std::optional<NavigationCapturingRedirectionInfo> redirection_info_;
};

class NavigationCapturingDataTransferBrowserTest
    : public WebAppBrowserTestBase {
 public:
  NavigationCapturingDataTransferBrowserTest() {
    std::map<std::string, std::string> parameters;
    parameters["link_capturing_state"] = "reimpl_default_on";
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing, parameters);
  }

  ~NavigationCapturingDataTransferBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  GURL GetStartUrl() {
    return embedded_test_server()->GetURL(kStartPageScopeA);
  }

  GURL GetDestinationUrl() {
    return embedded_test_server()->GetURL(kDestinationPageScopeB);
  }

  webapps::AppId InstallTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->launch_handler = blink::Manifest::LaunchHandler(
        blink::mojom::ManifestLaunchHandler_ClientMode::kAuto);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    const webapps::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    return app_id;
  }

  Browser* TriggerNavigationCapturingNewAppWindow(
      content::WebContents* contents,
      test::ClickMethod click,
      const std::string& elementId) {
    ui_test_utils::BrowserChangeObserver browser_added_waiter(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    test::SimulateClickOnElement(contents, elementId, click);

    Browser* app_browser = browser_added_waiter.Wait();
    EXPECT_NE(browser(), app_browser);
    return app_browser;
  }

  content::WebContents* OpenStartPageInApp(const webapps::AppId& app_id) {
    content::DOMMessageQueue message_queue;
    Browser* app_browser =
        web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    content::WebContents* contents =
        app_browser->tab_strip_model()->GetActiveWebContents();

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"FinishedNavigating\"", message);

    return contents;
  }

  content::WebContents* OpenStartPageInTab() {
    content::DOMMessageQueue message_queue;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kStartPageScopeA)));

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"FinishedNavigating\"", message);

    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  NavigationCapturingInformationForwarder* GetForwarderForWebContents(
      content::WebContents* contents) {
    if (!contents) {
      return nullptr;
    }
    return NavigationCapturingInformationForwarder::FromWebContents(contents);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NavigationCapturingDataTransferBrowserTest,
                       LeftClickNewWebContentsGetsCorrectDisposition) {
  const webapps::AppId app_id = InstallTestWebApp(GetDestinationUrl());

  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);
  NavigationCompletionAwaiter nav_awaiter(contents);

  Browser* app_browser = TriggerNavigationCapturingNewAppWindow(
      contents, test::ClickMethod::kLeftClick, kToSiteBTargetBlankNoopener);
  nav_awaiter.AwaitNavigationCompletion();
  ASSERT_NE(nullptr, app_browser);
  ASSERT_TRUE(nav_awaiter.GetRedirectionInfoForNavigation().has_value());

  NavigationCapturingRedirectionInfo redirection_info =
      *nav_awaiter.GetRedirectionInfoForNavigation();

  // Triggered from a tab and not an app window.
  EXPECT_FALSE(redirection_info.app_id_initial_browser.has_value());
  EXPECT_EQ(NavigationHandlingInitialResult::kAppWindowNavigationCaptured,
            redirection_info.initial_nav_handling_result);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
            redirection_info.disposition);
  EXPECT_TRUE(redirection_info.first_navigation_app_id.has_value());
  EXPECT_EQ(app_id, *redirection_info.first_navigation_app_id);

  // Post navigation, the WebContentsUserData instances should be cleaned up.
  EXPECT_THAT(GetForwarderForWebContents(
                  app_browser->tab_strip_model()->GetActiveWebContents()),
              testing::IsNull());
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingDataTransferBrowserTest,
                       LeftClickSameWebContentsGetsCorrectDisposition) {
  content::WebContents* contents = OpenStartPageInTab();
  ASSERT_NE(nullptr, contents);
  NavigationCompletionAwaiter nav_awaiter(contents);
  test::SimulateClickOnElement(contents, kToSiteATargetBlankWithOpener,
                               test::ClickMethod::kLeftClick);
  nav_awaiter.AwaitNavigationCompletion();
  ASSERT_TRUE(nav_awaiter.GetRedirectionInfoForNavigation().has_value());

  NavigationCapturingRedirectionInfo redirection_info =
      *nav_awaiter.GetRedirectionInfoForNavigation();

  // Triggered from a tab and not an app window.
  EXPECT_FALSE(redirection_info.app_id_initial_browser.has_value());
  EXPECT_EQ(NavigationHandlingInitialResult::kBrowserTab,
            redirection_info.initial_nav_handling_result);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
            redirection_info.disposition);
  EXPECT_FALSE(redirection_info.first_navigation_app_id.has_value());

  // Post navigation, the WebContentsUserData instances should be cleaned up.
  EXPECT_THAT(GetForwarderForWebContents(contents), testing::IsNull());
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingDataTransferBrowserTest,
                       ShiftClickNewWebContentsGetsCorrectDisposition) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());
  content::WebContents* contents = OpenStartPageInApp(app_id_a);
  ASSERT_NE(nullptr, contents);

  NavigationCompletionAwaiter nav_awaiter(contents);
  Browser* app_browser = TriggerNavigationCapturingNewAppWindow(
      contents, test::ClickMethod::kShiftClick, kToSiteBTargetBlankWithOpener);
  nav_awaiter.AwaitNavigationCompletion();
  ASSERT_NE(nullptr, app_browser);

  ASSERT_TRUE(nav_awaiter.GetRedirectionInfoForNavigation().has_value());

  NavigationCapturingRedirectionInfo redirection_info =
      *nav_awaiter.GetRedirectionInfoForNavigation();

  // Triggered from an app window for app_id_a.
  ASSERT_TRUE(redirection_info.app_id_initial_browser.has_value());
  EXPECT_EQ(app_id_a, *redirection_info.app_id_initial_browser);
  // Navigation capturing only extends to left clicks creating a capturable
  // experience, and does not extend to user modified clicks like shift and
  // middle clicks.
  EXPECT_EQ(NavigationHandlingInitialResult::kAppWindowForcedNewContext,
            redirection_info.initial_nav_handling_result);
  EXPECT_EQ(WindowOpenDisposition::NEW_WINDOW, redirection_info.disposition);
  ASSERT_TRUE(redirection_info.first_navigation_app_id.has_value());
  EXPECT_EQ(app_id_b, *redirection_info.first_navigation_app_id);

  EXPECT_THAT(GetForwarderForWebContents(
                  app_browser->tab_strip_model()->GetActiveWebContents()),
              testing::IsNull());
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingDataTransferBrowserTest,
                       MiddleClickNewWebContentsGetsCorrectDisposition) {
  const webapps::AppId app_id = InstallTestWebApp(GetStartUrl());
  content::WebContents* contents = OpenStartPageInApp(app_id);
  ASSERT_NE(nullptr, contents);

  NavigationCompletionAwaiter nav_awaiter(contents);

  Browser* app_browser = TriggerNavigationCapturingNewAppWindow(
      contents, test::ClickMethod::kMiddleClick, kToSiteATargetBlankWithOpener);
  nav_awaiter.AwaitNavigationCompletion();
  ASSERT_NE(nullptr, app_browser);
  ASSERT_TRUE(nav_awaiter.GetRedirectionInfoForNavigation().has_value());

  NavigationCapturingRedirectionInfo redirection_info =
      *nav_awaiter.GetRedirectionInfoForNavigation();

  // Triggered from an app window for app_id.
  ASSERT_TRUE(redirection_info.app_id_initial_browser.has_value());
  EXPECT_EQ(app_id, redirection_info.app_id_initial_browser.value());
  EXPECT_EQ(NavigationHandlingInitialResult::kAppWindowForcedNewContext,
            redirection_info.initial_nav_handling_result);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            redirection_info.disposition);
  ASSERT_TRUE(redirection_info.first_navigation_app_id.has_value());
  EXPECT_EQ(app_id, *redirection_info.first_navigation_app_id);

  EXPECT_THAT(GetForwarderForWebContents(
                  app_browser->tab_strip_model()->GetActiveWebContents()),
              testing::IsNull());
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingDataTransferBrowserTest,
                       AuxContentsDoNotEnqueueRedirectionInfo) {
  const webapps::AppId app_id_a = InstallTestWebApp(GetStartUrl());
  const webapps::AppId app_id_b = InstallTestWebApp(GetDestinationUrl());
  content::WebContents* contents = OpenStartPageInApp(app_id_a);
  ASSERT_NE(nullptr, contents);

  NavigationCompletionAwaiter nav_awaiter(contents);
  Browser* app_browser = TriggerNavigationCapturingNewAppWindow(
      contents, test::ClickMethod::kLeftClick, kToSiteBTargetBlankWithOpener);
  nav_awaiter.AwaitNavigationCompletion();

  ASSERT_NE(nullptr, app_browser);
  ASSERT_FALSE(nav_awaiter.GetRedirectionInfoForNavigation().has_value());
}

}  // namespace

}  // namespace web_app
