// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_insert/quick_insert_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/quick_insert/quick_insert_controller.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_list/search/test/test_ranker_manager.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Field;
using ::testing::IsSupersetOf;
using ::testing::VariantWith;

void AddSearchToHistory(Profile* profile,
                        GURL url,
                        base::Time last_visit = base::Time::Now()) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history->AddPageWithDetails(url, /*title=*/u"", /*visit_count=*/1,
                              /*typed_count=*/1,
                              /*last_visit=*/last_visit,
                              /*hidden=*/false, history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history);
}

void AddBookmarks(Profile* profile, std::u16string_view title, GURL url) {
  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                         std::u16string(title), url);
}

using QuickInsertClientImplBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(QuickInsertClientImplBrowserTest, StartCrosSearch) {
  // TODO(hidehiko): Take the instance of the real one from the system,
  // instead of setting up yet another instance here.
  ash::QuickInsertController controller;
  QuickInsertClientImpl client(g_browser_process->local_state(), &controller,
                               user_manager::UserManager::Get());
  AddSearchToHistory(GetProfile(), GURL("https://foo.com/history"));
  AddBookmarks(GetProfile(), u"Foobaz", GURL("https://foo.com/bookmarks"));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://foo.com/tab"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  browser()
      ->GetTabStripModel()
      ->GetWebContentsAt(1)
      ->GetController()
      .GetLastCommittedEntry()
      ->SetTitle(u"foo.com/tab");
  base::test::TestFuture<void> test_done;

  auto ranker_manager =
      std::make_unique<app_list::TestRankerManager>(GetProfile());
  ranker_manager->SetBestMatchString(u"tab");
  client.set_ranker_manager_for_test(std::move(ranker_manager));

  base::MockCallback<QuickInsertClientImpl::CrosSearchResultsCallback>
      mock_search_callback;
  EXPECT_CALL(mock_search_callback, Run(_, _)).Times(AnyNumber());
  EXPECT_CALL(
      mock_search_callback,
      Run(ash::AppListSearchResultType::kOmnibox,
          IsSupersetOf({
              VariantWith<ash::QuickInsertBrowsingHistoryResult>(AllOf(
                  Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                        GURL("https://foo.com/history")),
                  Field("best_match",
                        &ash::QuickInsertBrowsingHistoryResult::best_match,
                        false))),
              VariantWith<ash::QuickInsertBrowsingHistoryResult>(AllOf(
                  Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                        GURL("https://foo.com/tab")),
                  Field("best_match",
                        &ash::QuickInsertBrowsingHistoryResult::best_match,
                        true))),
              VariantWith<ash::QuickInsertBrowsingHistoryResult>(AllOf(
                  Field("title", &ash::QuickInsertBrowsingHistoryResult::title,
                        u"Foobaz"),
                  Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                        GURL("https://foo.com/bookmarks")),
                  Field("best_match",
                        &ash::QuickInsertBrowsingHistoryResult::best_match,
                        false))),
          })))
      .WillOnce([&]() { test_done.SetValue(); });

  client.StartCrosSearch(u"foo", /*category=*/std::nullopt,
                         mock_search_callback.Get());

  ASSERT_TRUE(test_done.Wait());
}

}  // namespace
