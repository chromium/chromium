// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/desktop_tab_model_url_visit_data_fetcher.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

using Source = URLVisit::Source;

class DesktopTabModelURLVisitDataFetcherTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    desktop_tab_model_url_visit_data_fetcher_ =
        std::make_unique<DesktopTabModelURLVisitDataFetcher>(GetProfile());
  }

  void TearDownOnMainThread() override {
    desktop_tab_model_url_visit_data_fetcher_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  FetchResult FetchAndGetResult(const FetchOptions& options) {
    FetchResult result = FetchResult(FetchResult::Status::kError, {});
    base::RunLoop wait_loop;
    desktop_tab_model_url_visit_data_fetcher_->FetchURLVisitData(
        options, FetcherConfig(),
        base::BindOnce(
            [](base::OnceClosure stop_waiting, FetchResult* result,
               FetchResult result_arg) {
              result->status = result_arg.status;
              result->data = std::move(result_arg.data);
              std::move(stop_waiting).Run();
            },
            wait_loop.QuitClosure(), &result));
    wait_loop.Run();
    return result;
  }

 protected:
  void AddTabWithTitle(Browser* browser,
                       const GURL url,
                       const std::string title) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    web_contents->UpdateTitleForEntry(
        web_contents->GetController().GetActiveEntry(),
        base::UTF8ToUTF16(title));
  }

 private:
  std::unique_ptr<DesktopTabModelURLVisitDataFetcher>
      desktop_tab_model_url_visit_data_fetcher_;
};

IN_PROC_BROWSER_TEST_F(DesktopTabModelURLVisitDataFetcherTest,
                       FetchURLVisitData) {
  size_t kSampleTabCount = 3;
  for (size_t i = 0; i < kSampleTabCount; i++) {
    AddTabWithTitle(browser(),
                    GURL(base::StringPrintf("https://www.google.com?q=%zu", i)),
                    base::NumberToString(kSampleTabCount));
  }

  auto options = FetchOptions(
      {
          {URLVisitAggregate::URLType::kActiveLocalTab,
           {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kTabModel, FetchOptions::FetchSources({Source::kLocal})},
      },
      base::Time::Now() - base::Days(1));
  auto result = FetchAndGetResult(options);
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 3u);
  const auto* tab_data =
      std::get_if<URLVisitAggregate::TabData>(&result.data.begin()->second);
  EXPECT_EQ(tab_data->last_active_tab.visit.source, Source::kLocal);
  EXPECT_EQ(tab_data->tab_count, 1u);
}

IN_PROC_BROWSER_TEST_F(DesktopTabModelURLVisitDataFetcherTest,
                       FetchURLVisitData_AggregateValues) {
  constexpr size_t kNumTabs = 3;
  // Create tab entries to be aggregated.
  for (size_t i = 0; i < kNumTabs; i++) {
    AddTabWithTitle(browser(), GURL("https://www.google.com?q=1"), "1");
  }

  // Change the order of the tabs so that the most recent one is last in the tab
  // strip. By doing so, we can validate the fetcher is retaining the last
  // active tab regardless of position.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // InProcessBrowserTest starts with one tab. The new tabs are added after it.
  tab_strip_model->MoveWebContentsAt(1, kNumTabs, true);

  auto options = FetchOptions(
      {
          {URLVisitAggregate::URLType::kActiveLocalTab,
           {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kTabModel, FetchOptions::FetchSources({Source::kLocal})},
      },
      base::Time::Now() - base::Days(1));
  auto result = FetchAndGetResult(options);
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 1u);
  const auto* tab_data =
      std::get_if<URLVisitAggregate::TabData>(&result.data.begin()->second);
  EXPECT_EQ(tab_data->last_active_tab.visit.source, Source::kLocal);
  EXPECT_EQ(tab_data->tab_count, kNumTabs);

  auto id = sessions::SessionTabHelper::IdForTab(
                tab_strip_model->GetActiveWebContents())
                .id();
  EXPECT_EQ(tab_data->last_active_tab.id, id);
}

}  // namespace visited_url_ranking
