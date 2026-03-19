// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class ProjectsPanelControllerBrowserTest : public InProcessBrowserTest {
 public:
  ProjectsPanelControllerBrowserTest() = default;
  ~ProjectsPanelControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    controller_ = std::make_unique<ProjectsPanelController>(
        browser(), /*state_controller=*/nullptr, &mock_tab_group_sync_service_,
        &mock_contextual_tasks_service_);
  }

  void TearDownOnMainThread() override {
    controller_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void InitializeControllerWithThread(const std::string& server_id,
                                      const base::Uuid& task_id) {
    contextual_tasks::ContextualTask task(task_id);
    task.AddThread(contextual_tasks::Thread(
        contextual_tasks::ThreadType::kAiMode, server_id, "Thread",
        /*last_turn_time_unix_epoch_millis=*/1, "turn_id"));

    std::vector<contextual_tasks::ContextualTask> tasks;
    tasks.push_back(task);

    EXPECT_CALL(mock_contextual_tasks_service_, GetTasks(testing::_))
        .WillOnce([tasks](base::OnceCallback<void(
                              std::vector<contextual_tasks::ContextualTask>)>
                              callback) mutable {
          std::move(callback).Run(std::move(tasks));
        });

    controller_->OnContextualTasksServiceInitialized();
  }

 protected:
  testing::NiceMock<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  testing::NiceMock<contextual_tasks::MockContextualTasksService>
      mock_contextual_tasks_service_;
  std::unique_ptr<ProjectsPanelController> controller_;
};

IN_PROC_BROWSER_TEST_F(ProjectsPanelControllerBrowserTest,
                       OpenThread_OpensNewTabInsteadOfActivatingExistingTab) {
  GURL thread_url("https://example.com/thread/123");
  const std::string server_id = "123";
  const base::Uuid task_id =
      base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000001");

  InitializeControllerWithThread(server_id, task_id);

  // Add a tab with the thread URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), thread_url));
  int thread_tab_index = browser()->tab_strip_model()->active_index();

  // Add another tab and activate it.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_NE(thread_tab_index, browser()->tab_strip_model()->active_index());

  int initial_tab_count = browser()->tab_strip_model()->count();

  // Mock the UI service to return the thread URL.
  EXPECT_CALL(mock_contextual_tasks_service_,
              GetThreadUrlFromTaskId(testing::Eq(task_id), testing::_,
                                     testing::_, testing::_))
      .WillOnce([thread_url](base::Uuid task_id, const std::string&,
                             omnibox::ChromeAimEntryPoint,
                             base::OnceCallback<void(GURL)> callback) {
        std::move(callback).Run(thread_url);
      });

  content::TestNavigationObserver navigation_observer(thread_url);
  navigation_observer.StartWatchingNewWebContents();

  controller_->OpenThread(server_id);

  navigation_observer.Wait();

  // Verify that a new tab was opened with the correct URL and is active.
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(thread_url, browser()
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ProjectsPanelControllerBrowserTest,
                       OpenThread_OpensNewTab) {
  GURL thread_url("https://example.com/thread/456");
  const std::string server_id = "456";
  const base::Uuid task_id =
      base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000001");

  InitializeControllerWithThread(server_id, task_id);

  int initial_tab_count = browser()->tab_strip_model()->count();

  // Mock the UI service to return the thread URL.
  EXPECT_CALL(mock_contextual_tasks_service_,
              GetThreadUrlFromTaskId(testing::Eq(task_id), testing::_,
                                     testing::_, testing::_))
      .WillOnce([thread_url](base::Uuid task_id, const std::string&,
                             omnibox::ChromeAimEntryPoint,
                             base::OnceCallback<void(GURL)> callback) {
        std::move(callback).Run(thread_url);
      });

  content::TestNavigationObserver navigation_observer(thread_url);
  navigation_observer.StartWatchingNewWebContents();

  controller_->OpenThread(server_id);

  navigation_observer.Wait();

  // Verify that a new tab was opened with the correct URL and is active.
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(thread_url, browser()
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL());
}
