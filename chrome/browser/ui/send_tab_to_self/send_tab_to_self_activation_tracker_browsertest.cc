// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_activation_tracker.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {
namespace {

class SendTabToSelfActivationTrackerBrowserTest : public InProcessBrowserTest {
 public:
  SendTabToSelfActivationTrackerBrowserTest() = default;
  ~SendTabToSelfActivationTrackerBrowserTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>();
        }));
  }

  FakeSendTabToSelfModel* GetModel(Profile* profile) {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile))
        ->GetFakeSendTabToSelfModel();
  }
};

// TODO(crbug.com/503283050): Re-enable these tests on ChromeOS. Session
// restore behaves differently or is flaky on ChromeOS integration tests.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SendTabToSelfActivationTrackerBrowserTest,
                       PRE_RestoreTrackerOnRestart) {
  // Enable session restore.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->GetProfile(), pref);

  // Tab 0 is active by default (about:blank).
  // Navigate Tab 0 to a specific URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));

  // Open Tab 1 in the background and wait for it to load.
  content::WebContents* tab1 = chrome::AddAndReturnTabAt(
      browser(), GURL("chrome://settings/"), -1, /*foreground=*/false);
  content::WaitForLoadStop(tab1);

  // Open Tab 2 in the background and wait for it to load. This is the target
  // tab.
  content::WebContents* target_contents = chrome::AddAndReturnTabAt(
      browser(), GURL("chrome://credits/"), -1, /*foreground=*/false);
  content::WaitForLoadStop(target_contents);

  // Ensure Tab 0 is still active.
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 3);

  // Attach the activation tracker to Tab 2.
  SendTabToSelfActivationTracker::CreateForWebContents(target_contents,
                                                       "test_guid");
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfActivationTrackerBrowserTest,
                       RestoreTrackerOnRestart) {
  TabStripModel* tab_strip = browser()->GetTabStripModel();

  // Expect at least 3 tabs.
  ASSERT_GE(tab_strip->count(), 3);

  // Find the target tab by URL "chrome://credits/".
  int target_index = -1;
  for (int i = 0; i < tab_strip->count(); ++i) {
    if (tab_strip->GetWebContentsAt(i)->GetURL() == GURL("chrome://credits/")) {
      target_index = i;
      break;
    }
  }
  ASSERT_NE(target_index, -1);

  // Ensure the target tab is in the background.
  ASSERT_NE(target_index, tab_strip->active_index());

  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  ASSERT_EQ(model->activated_call_count(), 0);

  // Activate the restored target tab.
  tab_strip->ActivateTabAt(target_index);

  // Verify that the model was notified of the activation.
  EXPECT_EQ(model->last_activated_guid(), "test_guid");
  EXPECT_EQ(model->last_activated_entry_point(),
            ShareActivatedEntryPoint::kTabStrip);
  EXPECT_EQ(model->activated_call_count(), 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SendTabToSelfActivationTrackerBrowserTest,
                       PRE_RestoreAndCloseTrackerOnRestart) {
  // Enable session restore.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->GetProfile(), pref);

  // Tab 0 is active by default (about:blank).
  // Navigate Tab 0 to a specific URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));

  // Open Tab 1 in the background and wait for it to load.
  content::WebContents* tab1 = chrome::AddAndReturnTabAt(
      browser(), GURL("chrome://settings/"), -1, /*foreground=*/false);
  content::WaitForLoadStop(tab1);

  // Open Tab 2 in the background and wait for it to load. This is the target
  // tab.
  content::WebContents* target_contents = chrome::AddAndReturnTabAt(
      browser(), GURL("chrome://credits/"), -1, /*foreground=*/false);
  content::WaitForLoadStop(target_contents);

  // Ensure Tab 0 is still active.
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 3);

  // Attach the activation tracker to Tab 2.
  SendTabToSelfActivationTracker::CreateForWebContents(target_contents,
                                                       "destroy_guid");
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfActivationTrackerBrowserTest,
                       RestoreAndCloseTrackerOnRestart) {
  TabStripModel* tab_strip = browser()->GetTabStripModel();

  // Expect at least 3 tabs.
  ASSERT_GE(tab_strip->count(), 3);

  // Find the target tab by URL "chrome://credits/".
  int target_index = -1;
  for (int i = 0; i < tab_strip->count(); ++i) {
    if (tab_strip->GetWebContentsAt(i)->GetURL() == GURL("chrome://credits/")) {
      target_index = i;
      break;
    }
  }
  ASSERT_NE(target_index, -1);

  // Ensure the target tab is in the background.
  ASSERT_NE(target_index, tab_strip->active_index());

  FakeSendTabToSelfModel* model = GetModel(browser()->GetProfile());
  ASSERT_EQ(model->activated_call_count(), 0);

  // Close the restored target tab without activating it.
  tab_strip->CloseWebContentsAt(target_index, TabCloseTypes::CLOSE_NONE);

  // Verify that the model was notified of the destruction (closed without
  // activation).
  EXPECT_EQ(model->last_activated_guid(), "destroy_guid");
  EXPECT_EQ(model->last_activated_entry_point(),
            ShareActivatedEntryPoint::kTabOrBrowserClosedWithoutActivation);
  EXPECT_EQ(model->activated_call_count(), 1);
}

}  // namespace
}  // namespace send_tab_to_self
