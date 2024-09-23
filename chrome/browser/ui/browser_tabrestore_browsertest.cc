// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabrestore.h"

#include <map>
#include <string>

#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

typedef InProcessBrowserTest BrowserTabRestoreTest;

void AwaitTabsReady(content::DOMMessageQueue* message_queue, int tabs) {
  for (int i = 0; i < tabs; ++i) {
    std::string message;
    EXPECT_TRUE(message_queue->WaitForMessage(&message));
    EXPECT_EQ("\"READY\"", message);
  }
}

void CheckVisbility(TabStripModel* tab_strip_model, int visible_index) {
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    const char kGetStateJS[] = "window.document.visibilityState;";
    std::string document_visibility_state =
        content::EvalJs(contents, kGetStateJS).ExtractString();
    if (i == visible_index) {
      EXPECT_EQ("visible", document_visibility_state);
    } else {
      EXPECT_EQ("hidden", document_visibility_state);
    }
  }
}

void CreateTestTabs(Browser* browser) {
  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("tab-restore-visibility.html"))));
  ui_test_utils::NavigateToURLWithDisposition(
      browser, test_page, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser, test_page, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

IN_PROC_BROWSER_TEST_F(BrowserTabRestoreTest, RecentTabsMenuTabDisposition) {
  // Create tabs.
  CreateTestTabs(browser());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, active_browser_list->size());

  // Close the first browser.
  const int active_tab_index = browser()->tab_strip_model()->active_index();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list->size());

  // Restore tabs using the browser's recent tabs menu.
  content::DOMMessageQueue queue;
  Browser* browser = active_browser_list->get(0);
  RecentTabsSubMenuModel menu(nullptr, browser);
  menu.ExecuteCommand(menu.GetFirstRecentTabsCommandId(), 0);
  // There should be 3 restored tabs in the new browser. The active tab should
  // be loading.
  EXPECT_EQ(2u, active_browser_list->size());
  Browser* restored_browser = active_browser_list->get(1);
  EXPECT_EQ(3, restored_browser->tab_strip_model()->count());
  EXPECT_TRUE(restored_browser->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetController()
                  .GetPendingEntry());
  AwaitTabsReady(&queue, 2);

  // For the two test tabs we've just received "READY" DOM message.
  // But there won't be such message from the "about:blank" tab.
  // And it is possible that TabLoader hasn't loaded it yet.
  // Thus we should wait for "load stop" event before we will perform
  // CheckVisbility on "about:blank".
  {
    content::WebContents* about_blank_contents =
        restored_browser->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ("about:blank", about_blank_contents->GetURL().spec());
    if (about_blank_contents->IsLoading() ||
        about_blank_contents->GetController().NeedsReload()) {
      content::LoadStopObserver load_stop_observer(about_blank_contents);
      load_stop_observer.Wait();
    }
  }

  // Previously active tab should have visible disposition.
  CheckVisbility(restored_browser->tab_strip_model(), active_tab_index);
}

// Expect a selected restored tab to start loading synchronously.
//
// Previously, on Mac, a selected restored tab only started loading when a
// native message indicated that the window was visible. On other platforms,
// it started loading synchronously. https://crbug.com/1022492
IN_PROC_BROWSER_TEST_F(BrowserTabRestoreTest,
                       SelectedRestoredTabStartsLoading) {
  sessions::SerializedNavigationEntry navigation_entry;
  navigation_entry.set_index(0);
  navigation_entry.set_virtual_url(GURL(url::kAboutBlankURL));

  std::vector<sessions::SerializedNavigationEntry> navigations;
  navigations.push_back(navigation_entry);

  content::WebContents* web_contents = chrome::AddRestoredTab(
      browser(), navigations, /* tab_index=*/1, /* selected_navigation=*/0,
      /* extension_app_id=*/std::string(), /* group=*/std::nullopt,
      /* select=*/true, /* pin=*/false,
      /* last_active_time_ticks=*/base::TimeTicks::Now(),
      /* last_active_time=*/base::Time::Now(),
      /* storage_namespace=*/nullptr,
      /* user_agent_override=*/sessions::SerializedUserAgentOverride(),
      /* extra_data*/ std::map<std::string, std::string>(),
      /* from_session_restore=*/true,
      /* is_active_browser=*/true);

  EXPECT_TRUE(web_contents->GetController().GetPendingEntry());
}

// Expect a *non* selected restored tab to *not* start loading synchronously.
IN_PROC_BROWSER_TEST_F(BrowserTabRestoreTest,
                       NonSelectedRestoredTabDoesNotStartsLoading) {
  sessions::SerializedNavigationEntry navigation_entry;
  navigation_entry.set_index(0);
  navigation_entry.set_virtual_url(GURL(url::kAboutBlankURL));

  std::vector<sessions::SerializedNavigationEntry> navigations;
  navigations.push_back(navigation_entry);

  content::WebContents* web_contents = chrome::AddRestoredTab(
      browser(), navigations, /* tab_index=*/1, /* selected_navigation=*/0,
      /* extension_app_id=*/std::string(), /* group=*/std::nullopt,
      /* select=*/false, /* pin=*/false,
      /* last_active_time_ticks=*/base::TimeTicks::Now(),
      /* last_active_time=*/base::Time::Now(),
      /* storage_namespace=*/nullptr,
      /* user_agent_override=*/sessions::SerializedUserAgentOverride(),
      /* extra_data*/ std::map<std::string, std::string>(),
      /* from_session_restore=*/true,
      /* is_active_browser=*/true);

  EXPECT_FALSE(web_contents->GetController().GetPendingEntry());
}

IN_PROC_BROWSER_TEST_F(BrowserTabRestoreTest, DelegateRestoreTabDisposition) {
  // Create tabs.
  CreateTestTabs(browser());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, active_browser_list->size());

  // Close the first browser.
  const int active_tab_index = browser()->tab_strip_model()->active_index();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list->size());

  // Check the browser has a delegated restore service.
  Browser* browser = active_browser_list->get(0);
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  bool has_tab_restore_service = !!service;
  ASSERT_TRUE(has_tab_restore_service);
  sessions::LiveTabContext* context =
      BrowserLiveTabContext::FindContextForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  bool has_live_tab_context = !!context;
  ASSERT_TRUE(has_live_tab_context);

  // Restore tabs using that delegated restore service.
  content::DOMMessageQueue queue;
  service->RestoreMostRecentEntry(context);
  AwaitTabsReady(&queue, 2);

  // There should be 3 restored tabs in the new browser.
  EXPECT_EQ(2u, active_browser_list->size());
  browser = active_browser_list->get(1);
  EXPECT_EQ(3, browser->tab_strip_model()->count());
  // The same as in RecentTabsMenuTabDisposition test case.
  // See there for the explanation.
  {
    content::WebContents* about_blank_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ("about:blank", about_blank_contents->GetURL().spec());
    if (about_blank_contents->IsLoading() ||
        about_blank_contents->GetController().NeedsReload()) {
      content::LoadStopObserver load_stop_observer(about_blank_contents);
      load_stop_observer.Wait();
    }
  }

  // Previously active tab should have visible disposition.
  CheckVisbility(browser->tab_strip_model(), active_tab_index);
}
