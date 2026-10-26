// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace send_tab_to_self {

namespace {

using base::test::TestFuture;

#if !BUILDFLAG(IS_LINUX)
// Waits until the specified browser window becomes inactive.
void WaitForBrowserToBecomeInactive(BrowserWindowInterface* browser) {
  if (!browser->IsActive()) {
    return;
  }
  TestFuture<BrowserWindowInterface*> inactive_future;
  base::CallbackListSubscription subscription =
      browser->RegisterDidBecomeInactive(
          inactive_future.GetRepeatingCallback());
  if (!browser->IsActive()) {
    return;
  }
  EXPECT_TRUE(inactive_future.Wait());
}
#endif  // !BUILDFLAG(IS_LINUX)

class SendTabToSelfToolbarIconControllerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser_view()->Activate();
    ui_test_utils::WaitForBrowserSetLastActive(browser());
  }

  void WaitUntilBrowserBecomeActiveOrLastActive(
      BrowserWindowInterface* browser) {
    ui_test_utils::WaitForBrowserSetLastActive(browser);
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  SendTabToSelfToolbarIconController* controller() {
    return static_cast<SendTabToSelfToolbarIconController*>(
        SendTabToSelfClientServiceFactory::GetForProfile(
            browser()->GetProfile())
            ->GetReceivingUiHandler());
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       ControllerExists) {
  EXPECT_TRUE(controller());
}

// Regression test for crbug.com/534231383.
IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       OpenPwaInBackgroundDoesNotCrash) {
  GURL app_url("https://www.example-a.com/app/index.html");
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->display_mode = blink::mojom::DisplayMode::kBrowser;

  webapps::AppId app_id = web_app::test::InstallWebApp(browser()->GetProfile(),
                                                       std::move(web_app_info));

  SendTabToSelfEntry entry("a", app_url, "PWA site", base::Time(), "device a",
                           "device b", PageContext(), NavigationHistory());

  base::WeakPtr<content::WebContents> opened_contents =
      OpenEntryInNewBackgroundTab(browser()->GetProfile(), entry);

  EXPECT_TRUE(opened_contents);
  EXPECT_EQ(app_url, opened_contents->GetURL());
}

class SendTabToSelfAutoOpenBrowserTest
    : public SendTabToSelfToolbarIconControllerTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>();
        }));
  }

  FakeSendTabToSelfModel* GetModel(Profile* profile) {
    SendTabToSelfClientServiceFactory::GetForProfile(profile);
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile))
        ->GetFakeSendTabToSelfModel();
  }
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfAutoOpenBrowserTest,
                       AutoOpenNewEntriesInForegroundIfActive) {
  ASSERT_TRUE(browser()->IsActive());

  base::HistogramTester histogram_tester;

  GURL url_1("https://www.example-a.com");
  GURL url_2("https://www.example-b.com");

  const int original_tab_count = browser()->GetTabStripModel()->count();
  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  model->SetLocalCacheGuid("device_b");

  TestFuture<void> activated_future;
  model->SetMarkEntryActivatedCallback(activated_future.GetRepeatingCallback());

  base::Time now = base::Time::Now();
  auto entries =
      model->AddEntriesRemotely({{.url = url_1,
                                  .title = "a site",
                                  .target_device_cache_guid = "device_b",
                                  .shared_time = now},
                                 {.url = url_2,
                                  .title = "b site",
                                  .target_device_cache_guid = "device_b",
                                  .shared_time = now + base::Seconds(1)}});
  const SendTabToSelfEntry* entry_1 = entries[0];

  EXPECT_EQ(original_tab_count + 2, browser()->GetTabStripModel()->count());
  // The new tabs are opened in the foreground, with the first incoming tab
  // (index 1) being the active one.
  EXPECT_EQ(url_1,
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(url_2,
            browser()->GetTabStripModel()->GetWebContentsAt(2)->GetURL());
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenedImmediately", true, 2);
  histogram_tester.ExpectBucketCount("Sharing.SendTabToSelf.AutoOpenOutcome3",
                                     AutoOpenOutcome::kTabOpenedInForeground,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Sharing.SendTabToSelf.AutoOpenOutcome3",
      AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);
  histogram_tester.ExpectTotalCount("Sharing.SendTabToSelf.AutoOpenOutcome3",
                                    2);

  // Tab activation dispatches visibility changes asynchronously on macOS.
  // Wait for the model activation callback before asserting.
  EXPECT_TRUE(activated_future.Wait());
  EXPECT_EQ(model->last_activated_guid(), entry_1->GetGUID());
  EXPECT_EQ(model->last_activated_entry_point(),
            ShareActivatedEntryPoint::kAutoOpened);
  EXPECT_EQ(model->activated_call_count(), 1);

  EXPECT_EQ(
      ToastService::From(browser())->toast_controller()->GetCurrentToastId(),
      ToastId::kSendTabToSelfTabOpened);
}

// This test cannot work on Wayland because the platform does not allow clients
// to position top level windows, activate them, and set focus.
#if !BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(SendTabToSelfAutoOpenBrowserTest,
                       AutoOpenPendingEntriesAsBackgroundTabsOnActivation) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support changing window activation "
                    "programmatically";
  }
#endif
  ASSERT_TRUE(browser()->IsActive());

  base::HistogramTester histogram_tester;

  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  WaitForBrowserToBecomeInactive(browser());
  ASSERT_FALSE(browser()->IsActive());

  GURL url_1("https://www.example-a.com");
  GURL url_2("https://www.example-b.com");

  const int original_tab_count = browser()->GetTabStripModel()->count();
  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  model->SetLocalCacheGuid("device_b");

  base::Time now = base::Time::Now();
  auto entries =
      model->AddEntriesRemotely({{.url = url_1,
                                  .title = "a site",
                                  .target_device_cache_guid = "device_b",
                                  .shared_time = now},
                                 {.url = url_2,
                                  .title = "b site",
                                  .target_device_cache_guid = "device_b",
                                  .shared_time = now + base::Seconds(1)}});
  const SendTabToSelfEntry* entry_1 = entries[0];

  // The entries should not be opened yet because the browser is inactive.
  EXPECT_EQ(original_tab_count, browser()->GetTabStripModel()->count());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenedImmediately", false, 2);
  histogram_tester.ExpectTotalCount("Sharing.SendTabToSelf.AutoOpenOutcome3",
                                    0);

  EXPECT_FALSE(
      ToastService::From(browser())->toast_controller()->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  EXPECT_EQ(original_tab_count + 2, browser()->GetTabStripModel()->count());
  // The new tabs are opened in the background (indices 1 and 2), and the active
  // index remains 0.
  EXPECT_EQ(url_1,
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(url_2,
            browser()->GetTabStripModel()->GetWebContentsAt(2)->GetURL());
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome3",
      AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation, 2);

  EXPECT_EQ(
      ToastService::From(browser())->toast_controller()->GetCurrentToastId(),
      ToastId::kSendTabToSelfTabsOpenedInBackground);

  TestFuture<void> activated_future;
  model->SetMarkEntryActivatedCallback(activated_future.GetRepeatingCallback());

  // Manually activate one of the background tabs (index 1) and verify the
  // model was notified.
  browser()->GetTabStripModel()->ActivateTabAt(1);

  // Tab activation dispatches visibility changes asynchronously on macOS.
  // Wait for the model activation callback before asserting.
  EXPECT_TRUE(activated_future.Wait());
  EXPECT_EQ(model->last_activated_guid(), entry_1->GetGUID());
  EXPECT_EQ(model->last_activated_entry_point(),
            ShareActivatedEntryPoint::kTabStrip);
  EXPECT_EQ(model->activated_call_count(), 1);
}

IN_PROC_BROWSER_TEST_F(
    SendTabToSelfAutoOpenBrowserTest,
    ToastActionButtonSwitchesToLatestTabsOpenedInBackground) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support changing window activation "
                    "programmatically";
  }
#endif
  ASSERT_TRUE(browser()->IsActive());
  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  WaitForBrowserToBecomeInactive(browser());
  ASSERT_FALSE(browser()->IsActive());

  GURL url_1("https://www.example-a.com");
  GURL url_2("https://www.example-b.com");

  const int original_tab_count = browser()->GetTabStripModel()->count();
  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  model->SetLocalCacheGuid("device_b");

  base::Time now = base::Time::Now();
  auto entries =
      model->AddEntriesRemotely({{.url = url_1,
                                  .title = "a site",
                                  .target_device_cache_guid = "device_b",
                                  .shared_time = now},
                                 {.url = url_2,
                                  .title = "b site",
                                  .target_device_cache_guid = "device_b",
                                  .shared_time = now + base::Seconds(1)}});
  const SendTabToSelfEntry* entry_1 = entries[0];

  ASSERT_FALSE(
      ToastService::From(browser())->toast_controller()->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  ASSERT_EQ(original_tab_count + 2, browser()->GetTabStripModel()->count());
  // The new tabs are opened in the background (indices 1 and 2), and the active
  // index remains 0.
  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());

  ASSERT_EQ(
      ToastService::From(browser())->toast_controller()->GetCurrentToastId(),
      ToastId::kSendTabToSelfTabsOpenedInBackground);

  TestFuture<void> activated_future;
  model->SetMarkEntryActivatedCallback(activated_future.GetRepeatingCallback());

  // Simulate clicking the toast action button.
  controller()->SwitchToLatestTabsOpenedInBackground(browser());
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());

  // Tab activation dispatches visibility changes asynchronously on macOS.
  // Wait for the model activation callback before asserting.
  EXPECT_TRUE(activated_future.Wait());
  EXPECT_EQ(model->last_activated_guid(), entry_1->GetGUID());
  EXPECT_EQ(model->last_activated_entry_point(),
            ShareActivatedEntryPoint::kDesktopToast);
  EXPECT_EQ(model->activated_call_count(), 1);
}

// This test covers an edge case scenario where a previously opened tab is
// closed before clicking the toast action button.
IN_PROC_BROWSER_TEST_F(
    SendTabToSelfAutoOpenBrowserTest,
    ToastActionButtonSwitchesToCorrectTabIfPreviousOneIsClosed) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support changing window activation "
                    "programmatically";
  }
#endif
  ASSERT_TRUE(browser()->IsActive());

  // Add a new tab.
  GURL url_1("https://www.example-a.com");
  chrome::AddTabAt(browser(), url_1, -1, true);
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  ASSERT_EQ(1, browser()->GetTabStripModel()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  WaitForBrowserToBecomeInactive(browser());
  ASSERT_FALSE(browser()->IsActive());

  GURL url_2("https://www.example-b.com");
  GURL url_3("https://www.example-c.com");

  const int original_tab_count = browser()->GetTabStripModel()->count();
  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  model->SetLocalCacheGuid("device_b");

  base::Time now = base::Time::Now();
  model->AddEntriesRemotely({{.url = url_2,
                              .title = "b site",
                              .target_device_cache_guid = "device_b",
                              .shared_time = now},
                             {.url = url_3,
                              .title = "c site",
                              .target_device_cache_guid = "device_b",
                              .shared_time = now + base::Seconds(1)}});

  ASSERT_FALSE(
      ToastService::From(browser())->toast_controller()->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  ASSERT_EQ(original_tab_count + 2, browser()->GetTabStripModel()->count());
  // The new tabs are opened in the background (indices 2 and 3), and the active
  // index remains 1.
  ASSERT_EQ(1, browser()->GetTabStripModel()->active_index());
  ASSERT_EQ(url_1,
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
  ASSERT_EQ(url_2,
            browser()->GetTabStripModel()->GetWebContentsAt(2)->GetURL());
  ASSERT_EQ(url_3,
            browser()->GetTabStripModel()->GetWebContentsAt(3)->GetURL());

  ASSERT_EQ(
      ToastService::From(browser())->toast_controller()->GetCurrentToastId(),
      ToastId::kSendTabToSelfTabsOpenedInBackground);

  // Close the previously active tab (index 1).
  browser()->GetTabStripModel()->CloseWebContentsAt(
      browser()->GetTabStripModel()->active_index(), TabCloseTypes::CLOSE_NONE);
  ASSERT_EQ(original_tab_count + 1, browser()->GetTabStripModel()->count());
  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());
  // The newly added tabs are now at indices 1 and 2 respectively.
  EXPECT_EQ(url_2,
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(url_3,
            browser()->GetTabStripModel()->GetWebContentsAt(2)->GetURL());

  // Simulate clicking the toast action button.
  controller()->SwitchToLatestTabsOpenedInBackground(browser());
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());
  EXPECT_EQ(url_2,
            browser()->GetTabStripModel()->GetActiveWebContents()->GetURL());
}

// This test covers an edge case scenario where the first of the newly opened
// tabs is closed before clicking the toast action button.
IN_PROC_BROWSER_TEST_F(
    SendTabToSelfAutoOpenBrowserTest,
    ToastActionButtonSwitchesToFirstAvailableNewTabAddedToBackground) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support changing window activation "
                    "programmatically";
  }
#endif
  ASSERT_TRUE(browser()->IsActive());
  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  WaitForBrowserToBecomeInactive(browser());
  ASSERT_FALSE(browser()->IsActive());

  GURL url_1("https://www.example-a.com");
  GURL url_2("https://www.example-b.com");

  const int original_tab_count = browser()->GetTabStripModel()->count();
  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  model->SetLocalCacheGuid("device_b");

  base::Time now = base::Time::Now();
  model->AddEntriesRemotely({{.url = url_1,
                              .title = "a site",
                              .target_device_cache_guid = "device_b",
                              .shared_time = now},
                             {.url = url_2,
                              .title = "b site",
                              .target_device_cache_guid = "device_b",
                              .shared_time = now + base::Seconds(1)}});

  ASSERT_FALSE(
      ToastService::From(browser())->toast_controller()->IsShowingToast());

  // Activate the browser and check that the entries are opened in the
  // background and the auto-open outcome is recorded.
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  ASSERT_EQ(original_tab_count + 2, browser()->GetTabStripModel()->count());
  // The new tabs are opened in the background (indices 1 and 2), and the active
  // index remains 0.
  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());

  ASSERT_EQ(
      ToastService::From(browser())->toast_controller()->GetCurrentToastId(),
      ToastId::kSendTabToSelfTabsOpenedInBackground);

  // Close the first of the newly opened tabs.
  browser()->GetTabStripModel()->CloseWebContentsAt(1,
                                                    TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(original_tab_count + 1, browser()->GetTabStripModel()->count());
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  // Simulate clicking the toast action button.
  controller()->SwitchToLatestTabsOpenedInBackground(browser());
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());
  EXPECT_EQ(url_2,
            browser()->GetTabStripModel()->GetActiveWebContents()->GetURL());
}

// Verifies that unopened entries persisted from a previous session are opened
// automatically on browser startup.
IN_PROC_BROWSER_TEST_F(SendTabToSelfAutoOpenBrowserTest, AutoOpenOnRestart) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support changing window activation "
                    "programmatically";
  }
#endif
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(browser()->IsActive());
  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());

  // Create an incognito browser and remove the current browser from focus.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);
  WaitForBrowserToBecomeInactive(browser());
  ASSERT_FALSE(browser()->IsActive());

  GURL url_1("https://www.example-a.com");
  GURL url_2("https://www.example-b.com");

  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  model->SetLocalCacheGuid("device_b");
  base::Time now = base::Time::Now();
  model->AddEntriesRemotely({{.url = url_1,
                              .title = "a site",
                              .target_device_cache_guid = "device_b",
                              .shared_time = now},
                             {.url = url_2,
                              .title = "b site",
                              .target_device_cache_guid = "device_b",
                              .shared_time = now + base::Seconds(1)}});

  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenedImmediately", false, 2);
  histogram_tester.ExpectTotalCount("Sharing.SendTabToSelf.AutoOpenOutcome3",
                                    0);

  // Open a new browser with the same profile.
  BrowserWindowInterface* new_browser = CreateBrowser(browser()->GetProfile());
  WaitUntilBrowserBecomeActiveOrLastActive(new_browser);

  // The pending entries should open automatically in the new browser.
  EXPECT_EQ(3, new_browser->GetTabStripModel()->count());
  // The new tabs are opened in the background (indices 1 and 2), and the active
  // index remains 0.
  EXPECT_EQ(GURL("https://www.example-a.com"),
            new_browser->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(GURL("https://www.example-b.com"),
            new_browser->GetTabStripModel()->GetWebContentsAt(2)->GetURL());
  EXPECT_EQ(0, new_browser->GetTabStripModel()->active_index());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome3",
      AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation, 2);
}
#endif  // !BUILDFLAG(IS_LINUX)

}  // namespace

}  // namespace send_tab_to_self
