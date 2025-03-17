// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/browsing_history_handler.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/sync/base/data_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"
#include "url/gurl.h"

using testing::_;

namespace history {

class MockBrowsingHistoryService : public BrowsingHistoryService {
 public:
  MOCK_METHOD(void,
              QueryHistory,
              (const std::u16string& search_text, const QueryOptions& options),
              (override));
};

namespace {

base::Time PretendNow() {
  static constexpr base::Time::Exploded kReferenceTime = {.year = 2015,
                                                          .month = 1,
                                                          .day_of_week = 5,
                                                          .day_of_month = 2,
                                                          .hour = 11,
                                                          .minute = 0,
                                                          .second = 0,
                                                          .millisecond = 0};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kReferenceTime, &out_time));
  return out_time;
}

class BrowsingHistoryHandlerWithWebUIForTesting
    : public BrowsingHistoryHandler {
 public:
  explicit BrowsingHistoryHandlerWithWebUIForTesting(
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents)
      : BrowsingHistoryHandler(std::move(pending_page_handler),
                               profile,
                               web_contents) {
    set_clock(&test_clock_);
    test_clock_.SetNow(PretendNow());
    auto service = std::make_unique<
        testing::StrictMock<history::MockBrowsingHistoryService>>();
    set_browsing_history_service_for_testing(std::move(service));
  }

  BrowsingHistoryHandlerWithWebUIForTesting(
      const BrowsingHistoryHandlerWithWebUIForTesting&) = delete;
  BrowsingHistoryHandlerWithWebUIForTesting& operator=(
      const BrowsingHistoryHandlerWithWebUIForTesting&) = delete;

  base::SimpleTestClock* test_clock() { return &test_clock_; }
  history::MockBrowsingHistoryService* mock_service() {
    return static_cast<history::MockBrowsingHistoryService*>(
        get_browsing_history_service_for_testing());
  }

 private:
  base::SimpleTestClock test_clock_;
};

}  // namespace

class BrowsingHistoryHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());

    handler_ = std::make_unique<BrowsingHistoryHandlerWithWebUIForTesting>(
        mojo::PendingReceiver<history::mojom::PageHandler>(), profile(),
        web_contents());
  }

  void MockHistoryServiceCall(
      const std::u16string& search_text,
      const QueryOptions& options,
      std::vector<BrowsingHistoryService::HistoryEntry> mock_results = {}) {
    EXPECT_CALL(
        *handler_->mock_service(),
        QueryHistory(search_text,
                     ::testing::FieldsAre(
                         /*begin_time*/ options.begin_time,
                         /*end_time*/ options.end_time,
                         /*max_count*/ 150,
                         /*duplicate_policy*/
                         history::QueryOptions::REMOVE_DUPLICATES_PER_DAY,
                         /*matching_algorithm*/ options.matching_algorithm,
                         /*host_only*/ options.host_only,
                         /*visit_order*/ options.visit_order,
                         /*app_id*/ options.app_id)))
        .Times(1)
        .WillOnce(testing::Invoke([&, mock_results](
                                      const std::u16string& search_text,
                                      const QueryOptions& options) {
          std::vector<BrowsingHistoryService::HistoryEntry> results;
          if (mock_results.empty()) {
            BrowsingHistoryService::HistoryEntry entry(
                BrowsingHistoryService::HistoryEntry::LOCAL_ENTRY,
                GURL(("http://test.com")), u"Test",
                base::Time::Now() - base::Minutes(5), std::string(), false,
                std::u16string(), false, GURL(), 0, 0, history::kNoAppIdFilter);
            results.push_back(entry);
          }

          BrowsingHistoryService::QueryResultsInfo info;
          info.search_text = search_text;
          info.reached_beginning = true;
          info.sync_timed_out = false;
          info.has_synced_results = true;
          handler_->OnQueryComplete(
              mock_results.empty() ? results : mock_results, info,
              base::OnceClosure());
        }));
  }

  mojom::QueryResultPtr RunQueryHistory(
      const std::string& query,
      std::optional<double> begin_timestamp = std::nullopt) {
    mojom::QueryResultPtr history_query_results;
    base::RunLoop run_loop;
    handler_->QueryHistory(
        query, 150, begin_timestamp,
        base::BindLambdaForTesting([&](history::mojom::QueryResultPtr result) {
          history_query_results = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return history_query_results;
  }

  void TearDown() override {
    handler_.reset();
    web_ui_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            BookmarkModelFactory::GetInstance(),
            BookmarkModelFactory::GetDefaultFactory()},
    };
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  BrowsingHistoryHandlerWithWebUIForTesting* handler() {
    return handler_.get();
  }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<BrowsingHistoryHandlerWithWebUIForTesting> handler_;
};

TEST_F(BrowsingHistoryHandlerTest, HostPrefixParameter) {
  std::u16string query = u"www.chromium.org";
  QueryOptions options;
  options.host_only = true;
  MockHistoryServiceCall(query, options);

  RunQueryHistory("host:www.chromium.org");
}

TEST_F(BrowsingHistoryHandlerTest, WithoutHostPrefixParameter) {
  std::u16string query = u"www.chromium.org";
  QueryOptions options;
  options.host_only = false;
  MockHistoryServiceCall(query, options);

  RunQueryHistory("www.chromium.org");
}

TEST_F(BrowsingHistoryHandlerTest, MisplacedHostPrefixParameter) {
  {
    std::u16string query = u"whost:ww.chromium.org";
    QueryOptions options;
    options.host_only = false;
    MockHistoryServiceCall(query, options);

    RunQueryHistory("whost:ww.chromium.org");
  }

  {
    std::u16string query = u"www.chromium.orghost:";
    QueryOptions options;
    options.host_only = false;
    MockHistoryServiceCall(query, options);

    RunQueryHistory("www.chromium.orghost:");
  }
}

TEST_F(BrowsingHistoryHandlerTest, BeginTimestamp) {
  {
    std::u16string query = u"query";
    double timestamp = 1713546406359L;
    QueryOptions options;
    options.begin_time = base::Time::FromMillisecondsSinceUnixEpoch(timestamp);
    MockHistoryServiceCall(query, options);
    RunQueryHistory("query", timestamp);
  }

  {
    std::u16string query = u"www.chromium.orghost:";
    QueryOptions options;
    options.host_only = false;
    MockHistoryServiceCall(query, options);
    RunQueryHistory("www.chromium.orghost:");
  }
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(BrowsingHistoryHandlerTest, MdTruncatesTitles) {
  std::vector<BrowsingHistoryService::HistoryEntry> results;
  history::BrowsingHistoryService::HistoryEntry long_url_entry;
  long_url_entry.url = GURL(
      "http://loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "ngurlislong.com");
  results.push_back(long_url_entry);
  ASSERT_GT(long_url_entry.url.spec().size(), 300U);
  QueryOptions options;
  MockHistoryServiceCall(u"test", options, results);
  auto results_mojom = RunQueryHistory("test");

  ASSERT_EQ(0u, results_mojom->value[0]->title.find("http://loooo"));
  EXPECT_EQ(300u, results_mojom->value[0]->title.size());
}
#endif

}  // namespace history
