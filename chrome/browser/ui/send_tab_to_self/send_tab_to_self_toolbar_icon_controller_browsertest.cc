// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"

namespace send_tab_to_self {

class SendTabToSelfToolbarIconControllerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ui_test_utils::WaitForBrowserSetLastActive(browser());
  }

  void WaitUntilBrowserBecomeActiveOrLastActive(Browser* browser) {
    ui_test_utils::WaitForBrowserSetLastActive(browser);
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  SendTabToSelfToolbarIconController* controller() {
    return static_cast<SendTabToSelfToolbarIconController*>(
        SendTabToSelfClientServiceFactory::GetForProfile(browser()->profile())
            ->GetReceivingUiHandler());
  }

  SendTabToSelfToolbarBubbleController* bubble_controller() {
    return SendTabToSelfToolbarBubbleController::From(browser());
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       DisplayNewEntry) {
  ASSERT_TRUE(browser()->IsActive());

  SendTabToSelfEntry entry("a", GURL("https://www.example-a.com"), "a site",
                           base::Time(), "device a", "device b", PageContext(),
                           NavigationHistory());

  controller()->DisplayNewEntries({&entry});
  EXPECT_TRUE(bubble_controller()->IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       ControllerExists) {
  EXPECT_TRUE(controller());
}

// This test cannot work on Wayland because the platform does not allow clients
// to position top level windows, activate them, and set focus.
#if !(BUILDFLAG(SUPPORTS_OZONE_WAYLAND) || BUILDFLAG(IS_CHROMEOS))
IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       StorePendingNewEntryFromIncognitoBrowser) {
  ASSERT_TRUE(browser()->IsActive());

  Browser* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);

  SendTabToSelfEntry entry("a", GURL("https://www.example-a.com"), "a site",
                           base::Time(), "device a", "device b", PageContext(),
                           NavigationHistory());

  EXPECT_FALSE(browser()->IsActive());
  controller()->DisplayNewEntries({&entry});
  EXPECT_FALSE(bubble_controller()->IsBubbleShowing());

  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());
  EXPECT_TRUE(bubble_controller()->IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       StorePendingNewEntryFromWebApp) {
  ASSERT_TRUE(browser()->IsActive());
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.org/"));
  webapps::AppId app_id = web_app::test::InstallWebApp(browser()->profile(),
                                                       std::move(web_app_info));
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  app_browser->GetBrowserView().Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(app_browser);

  SendTabToSelfEntry entry("a", GURL("https://www.example-a.com"), "a site",
                           base::Time(), "device a", "device b", PageContext(),
                           NavigationHistory());

  EXPECT_FALSE(browser()->IsActive());
  controller()->DisplayNewEntries({&entry});
  EXPECT_FALSE(bubble_controller()->IsBubbleShowing());

  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());
  EXPECT_TRUE(bubble_controller()->IsBubbleShowing());
}
#endif

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       ReplaceExistingEntry) {
  controller()->set_ignore_active_for_testing(true);
  SendTabToSelfEntry existing_entry(
      "a", GURL("https://www.example-a.com"), "a site", base::Time(),
      "device a", "device b", PageContext(), NavigationHistory());
  SendTabToSelfEntry new_entry("b", GURL("https://www.example-b.com"), "b site",
                               base::Time(), "device a", "device b",
                               PageContext(), NavigationHistory());

  controller()->DisplayNewEntries({&existing_entry});
  EXPECT_EQ(existing_entry.GetGUID(),
            bubble_controller()->bubble()->GetGuidForTesting());

  // For some reason, displaying the initial bubble seems to deactivate the
  // browser
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  controller()->DisplayNewEntries({&new_entry});
  EXPECT_EQ(new_entry.GetGUID(),
            bubble_controller()->bubble()->GetGuidForTesting());
}

class SendTabToSelfToolbarIconControllerAutoOpenTest
    : public SendTabToSelfToolbarIconControllerTest {
 public:
  void SetUpOnMainThread() override {
    SendTabToSelfToolbarIconControllerTest::SetUpOnMainThread();
    browser_view()->Activate();
    WaitUntilBrowserBecomeActiveOrLastActive(browser());
  }

 private:
  base::test::ScopedFeatureList feature_list_{kSendTabToSelfAutoOpen};
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerAutoOpenTest,
                       AutoOpenNewEntriesInForegroundIfActive) {
  ASSERT_TRUE(browser()->IsActive());

  base::HistogramTester histogram_tester;

  GURL url_1("https://www.example-a.com");
  SendTabToSelfEntry entry_1("new_entry_1", url_1, "a site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());
  GURL url_2("https://www.example-b.com");
  SendTabToSelfEntry entry_2("new_entry_2", url_2, "b site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());

  const int original_tab_count = browser()->tab_strip_model()->count();
  controller()->DisplayNewEntries({&entry_1, &entry_2});

  EXPECT_FALSE(bubble_controller()->IsBubbleShowing());
  EXPECT_EQ(original_tab_count + 2, browser()->tab_strip_model()->count());
  // The new tabs are opened in the foreground, with the first incoming tab
  // (index 1) being the active one.
  EXPECT_EQ(url_1, browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(url_2, browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  histogram_tester.ExpectUniqueSample("Sharing.SendTabToSelf.AutoOpenOutcome",
                                      AutoOpenOutcome::kSuccess, 2);

  EXPECT_EQ(browser()
                ->browser_window_features()
                ->toast_service()
                ->toast_controller()
                ->GetCurrentToastId(),
            ToastId::kSendTabToSelfTabOpened);
}

// This test cannot work on Wayland because the platform does not allow clients
// to position top level windows, activate them, and set focus.
#if !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerAutoOpenTest,
                       AutoOpenPendingEntriesAsBackgroundTabsOnActivation) {
  ASSERT_TRUE(browser()->IsActive());

  base::HistogramTester histogram_tester;

  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  Browser* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  ASSERT_FALSE(browser()->IsActive());

  GURL url_1("https://www.example-a.com");
  SendTabToSelfEntry entry_1("new_entry_1", url_1, "a site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());
  GURL url_2("https://www.example-b.com");
  SendTabToSelfEntry entry_2("new_entry_2", url_2, "b site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());

  const int original_tab_count = browser()->tab_strip_model()->count();
  controller()->DisplayNewEntries({&entry_1, &entry_2});

  EXPECT_EQ(original_tab_count, browser()->tab_strip_model()->count());

  histogram_tester.ExpectUniqueSample("Sharing.SendTabToSelf.AutoOpenOutcome",
                                      AutoOpenOutcome::kPending, 2);

  EXPECT_FALSE(browser()
                   ->browser_window_features()
                   ->toast_service()
                   ->toast_controller()
                   ->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  EXPECT_FALSE(bubble_controller()->IsBubbleShowing());

  EXPECT_EQ(original_tab_count + 2, browser()->tab_strip_model()->count());
  // The new tabs are opened in the background (indices 1 and 2), and the active
  // index remains 0.
  EXPECT_EQ(url_1, browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(url_2, browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  histogram_tester.ExpectBucketCount("Sharing.SendTabToSelf.AutoOpenOutcome",
                                     AutoOpenOutcome::kOpenedPending, 2);

  EXPECT_EQ(browser()
                ->browser_window_features()
                ->toast_service()
                ->toast_controller()
                ->GetCurrentToastId(),
            ToastId::kSendTabToSelfTabsOpenedInBackground);
}

IN_PROC_BROWSER_TEST_F(
    SendTabToSelfToolbarIconControllerAutoOpenTest,
    ToastActionButtonSwitchesToLatestTabsOpenedInBackground) {
  ASSERT_TRUE(browser()->IsActive());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  Browser* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  ASSERT_FALSE(browser()->IsActive());

  GURL url_1("https://www.example-a.com");
  SendTabToSelfEntry entry_1("new_entry_1", url_1, "a site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());
  GURL url_2("https://www.example-b.com");
  SendTabToSelfEntry entry_2("new_entry_2", url_2, "b site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());

  const int original_tab_count = browser()->tab_strip_model()->count();
  controller()->DisplayNewEntries({&entry_1, &entry_2});

  ASSERT_FALSE(browser()
                   ->browser_window_features()
                   ->toast_service()
                   ->toast_controller()
                   ->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  ASSERT_EQ(original_tab_count + 2, browser()->tab_strip_model()->count());
  // The new tabs are opened in the background (indices 1 and 2), and the active
  // index remains 0.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_EQ(browser()
                ->browser_window_features()
                ->toast_service()
                ->toast_controller()
                ->GetCurrentToastId(),
            ToastId::kSendTabToSelfTabsOpenedInBackground);

  // Simulate clicking the toast action button.
  controller()->SwitchToLatestTabsOpenedInBackground(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
}

// This test covers an edge case scenario where a previously opened tab is
// closed before clicking the toast action button.
IN_PROC_BROWSER_TEST_F(
    SendTabToSelfToolbarIconControllerAutoOpenTest,
    ToastActionButtonSwitchesToCorrectTabIfPreviousOneIsClosed) {
  ASSERT_TRUE(browser()->IsActive());

  // Add a new tab.
  GURL url_1("https://www.example-a.com");
  chrome::AddTabAt(browser(), url_1, -1, true);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  Browser* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  ASSERT_FALSE(browser()->IsActive());

  GURL url_2("https://www.example-b.com");
  SendTabToSelfEntry entry_2("new_entry_2", url_2, "b site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());
  GURL url_3("https://www.example-c.com");
  SendTabToSelfEntry entry_3("new_entry_3", url_3, "c site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());

  const int original_tab_count = browser()->tab_strip_model()->count();
  controller()->DisplayNewEntries({&entry_2, &entry_3});

  ASSERT_FALSE(browser()
                   ->browser_window_features()
                   ->toast_service()
                   ->toast_controller()
                   ->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  ASSERT_EQ(original_tab_count + 2, browser()->tab_strip_model()->count());
  // The new tabs are opened in the background (indices 2 and 3), and the active
  // index remains 1.
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(url_1, browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  ASSERT_EQ(url_2, browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());
  ASSERT_EQ(url_3, browser()->tab_strip_model()->GetWebContentsAt(3)->GetURL());

  ASSERT_EQ(browser()
                ->browser_window_features()
                ->toast_service()
                ->toast_controller()
                ->GetCurrentToastId(),
            ToastId::kSendTabToSelfTabsOpenedInBackground);

  // Close the previously active tab (index 1).
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), TabCloseTypes::CLOSE_NONE);
  ASSERT_EQ(original_tab_count + 1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  // The newly added tabs are now at indices 1 and 2 respectively.
  EXPECT_EQ(url_2, browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(url_3, browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());

  // Simulate clicking the toast action button.
  controller()->SwitchToLatestTabsOpenedInBackground(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(url_2,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// This test covers an edge case scenario where the first of the newly opened
// tabs is closed before clicking the toast action button.
IN_PROC_BROWSER_TEST_F(
    SendTabToSelfToolbarIconControllerAutoOpenTest,
    ToastActionButtonSwitchesToFirstAvailableNewTabAddedToBackground) {
  ASSERT_TRUE(browser()->IsActive());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  Browser* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  ASSERT_FALSE(browser()->IsActive());

  GURL url_1("https://www.example-a.com");
  SendTabToSelfEntry entry_1("new_entry_1", url_1, "a site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());
  GURL url_2("https://www.example-b.com");
  SendTabToSelfEntry entry_2("new_entry_2", url_2, "b site", base::Time::Now(),
                             "device a", "device b", PageContext(),
                             NavigationHistory());

  const int original_tab_count = browser()->tab_strip_model()->count();
  controller()->DisplayNewEntries({&entry_1, &entry_2});

  ASSERT_FALSE(browser()
                   ->browser_window_features()
                   ->toast_service()
                   ->toast_controller()
                   ->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  ASSERT_EQ(original_tab_count + 2, browser()->tab_strip_model()->count());
  // The new tabs are opened in the background (indices 1 and 2), and the active
  // index remains 0.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_EQ(browser()
                ->browser_window_features()
                ->toast_service()
                ->toast_controller()
                ->GetCurrentToastId(),
            ToastId::kSendTabToSelfTabsOpenedInBackground);

  // Close the first of the newly opened tabs.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(original_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Simulate clicking the toast action button.
  controller()->SwitchToLatestTabsOpenedInBackground(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(url_2,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}
#endif  // !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)

}  // namespace send_tab_to_self
