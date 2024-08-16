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
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace history {

class MockBrowsingHistoryService : public BrowsingHistoryService {
 public:
  MOCK_METHOD(void, QueryHistory,
              (const std::u16string& search_text, const QueryOptions& options),
              (override));
};

}  // namespace history

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
  explicit BrowsingHistoryHandlerWithWebUIForTesting(content::WebUI* web_ui) {
    set_clock(&test_clock_);
    set_web_ui(web_ui);
    test_clock_.SetNow(PretendNow());
    auto service = std::make_unique<
        testing::StrictMock<history::MockBrowsingHistoryService>>();
    mock_service_ = service.get();
    set_browsing_history_service_for_testing(std::move(service));
  }

  BrowsingHistoryHandlerWithWebUIForTesting(
      const BrowsingHistoryHandlerWithWebUIForTesting&) = delete;
  BrowsingHistoryHandlerWithWebUIForTesting& operator=(
      const BrowsingHistoryHandlerWithWebUIForTesting&) = delete;

  void SendHistoryQuery(int count, const std::u16string& query,
                        std::optional<double> begin_timestamp) override {
    if (postpone_query_results_) {
      return;
    }
    BrowsingHistoryHandler::SendHistoryQuery(count, query, begin_timestamp);
  }

  void PostponeResults() { postpone_query_results_ = true; }

  base::SimpleTestClock* test_clock() { return &test_clock_; }
  history::MockBrowsingHistoryService* mock_service() { return mock_service_; }

 private:
  base::SimpleTestClock test_clock_;
  bool postpone_query_results_ = false;
  raw_ptr<history::MockBrowsingHistoryService, DanglingUntriaged> mock_service_;
};

}  // namespace

class BrowsingHistoryHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
    web_history_service_ = static_cast<history::FakeWebHistoryService*>(
        WebHistoryServiceFactory::GetForProfile(profile()));
    ASSERT_TRUE(web_history_service_);

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
  }

  void TearDown() override {
    web_ui_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            SyncServiceFactory::GetInstance(),
            base::BindRepeating(&BuildTestSyncService)},
        TestingProfile::TestingFactory{
            WebHistoryServiceFactory::GetInstance(),
            base::BindRepeating(&BuildFakeWebHistoryService)},
        TestingProfile::TestingFactory{
            BookmarkModelFactory::GetInstance(),
            BookmarkModelFactory::GetDefaultFactory()},
    };
  }

  void VerifyHistoryDeletedFired(content::TestWebUI::CallData& data) {
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("history-deleted", data.arg1()->GetString());
  }

  void InitializeWebUI(BrowsingHistoryHandlerWithWebUIForTesting& handler) {
    // Send historyLoaded so that JS will be allowed.
    base::Value::List init_args;
    init_args.Append("query-history-callback-id");
    init_args.Append("");
    init_args.Append(150);
    handler.HandleQueryHistory(init_args);
  }

  syncer::TestSyncService* sync_service() { return sync_service_; }
  history::WebHistoryService* web_history_service() {
    return web_history_service_;
  }
  content::TestWebUI* web_ui() { return web_ui_.get(); }

 private:
  static std::unique_ptr<KeyedService> BuildTestSyncService(
      content::BrowserContext* context) {
    return std::make_unique<syncer::TestSyncService>();
  }

  static std::unique_ptr<KeyedService> BuildFakeWebHistoryService(
      content::BrowserContext* context) {
    std::unique_ptr<history::FakeWebHistoryService> service =
        std::make_unique<history::FakeWebHistoryService>();
    service->SetupFakeResponse(true /* success */, net::HTTP_OK);
    return service;
  }

  raw_ptr<syncer::TestSyncService, DanglingUntriaged> sync_service_ = nullptr;
  raw_ptr<history::FakeWebHistoryService, DanglingUntriaged>
      web_history_service_ = nullptr;
  std::unique_ptr<content::TestWebUI> web_ui_;
};

// Tests that BrowsingHistoryHandler is informed about WebHistoryService
// deletions.
TEST_F(BrowsingHistoryHandlerTest, ObservingWebHistoryDeletions) {
  base::RepeatingCallback<void(bool)> callback = base::DoNothing();

  // BrowsingHistoryHandler is informed about WebHistoryService history
  // deletions.
  {
    ASSERT_EQ(sync_service()->GetTransportState(),
              syncer::SyncService::TransportState::ACTIVE);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();
    handler.StartQueryHistory();
    InitializeWebUI(handler);

    // QueryHistory triggers 2 calls to HasOtherFormsOfBrowsingHistory that fire
    // before the callback is resolved if the sync service is active when the
    // first query is sent. The handler should also resolve the initial
    // queryHistory callback.
    EXPECT_EQ(3U, web_ui()->call_data().size());

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(), callback,
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

    EXPECT_EQ(4U, web_ui()->call_data().size());
    VerifyHistoryDeletedFired(*web_ui()->call_data().back());
  }

  // BrowsingHistoryHandler will be informed about WebHistoryService deletions
  // even if history sync is activated later.
  {
    sync_service()->SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();
    handler.StartQueryHistory();
    sync_service()->SetMaxTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service()->FireStateChanged();

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(), callback,
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_EQ(4U, web_ui()->call_data().size());

    // Simulate initialization after history has been deleted. The
    // history-deleted event will happen before the historyResults() callback,
    // since AllowJavascript is called before returning the results.
    InitializeWebUI(handler);

    // QueryHistory triggers 1 call to HasOtherFormsOfBrowsingHistory that fire
    // before the callback is resolved if the sync service is inactive when the
    // first query is sent. The handler should also have fired history-deleted
    // and resolved the initial queryHistory callback.
    EXPECT_EQ(7U, web_ui()->call_data().size());
    VerifyHistoryDeletedFired(
        *web_ui()->call_data()[web_ui()->call_data().size() - 2]);
  }

  // BrowsingHistoryHandler does not fire historyDeleted while a web history
  // delete request is happening.
  {
    ASSERT_EQ(sync_service()->GetTransportState(),
              syncer::SyncService::TransportState::ACTIVE);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();
    handler.StartQueryHistory();
    InitializeWebUI(handler);
    // QueryHistory triggers 2 calls to HasOtherFormsOfBrowsingHistory that fire
    // before the callback is resolved if the sync service is active when the
    // first query is sent. The handler should also resolve the initial
    // queryHistory callback.
    EXPECT_EQ(10U, web_ui()->call_data().size());

    // Simulate a delete request.
    base::Value::List args;
    args.Append("remove-visits-callback-id");
    base::Value::List to_remove;
    base::Value::Dict visit;
    visit.Set("url", "https://www.google.com");
    base::Value::List timestamps;
    timestamps.Append(12345678.0);
    visit.Set("timestamps", std::move(timestamps));
    to_remove.Append(std::move(visit));
    args.Append(std::move(to_remove));
    handler.HandleRemoveVisits(args);

    EXPECT_EQ(11U, web_ui()->call_data().size());
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("remove-visits-callback-id", data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2()->GetBool());
  }

  // When history sync is not active, we don't listen to WebHistoryService
  // deletions. The WebHistoryService object still exists (because it's a
  // BrowserContextKeyedService), but is not visible to BrowsingHistoryHandler.
  {
    sync_service()->SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();
    handler.StartQueryHistory();

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(), callback,
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

    // No additional WebUI calls were made.
    EXPECT_EQ(11U, web_ui()->call_data().size());
  }
}

TEST_F(BrowsingHistoryHandlerTest, HostPrefixParameter) {
  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  ASSERT_TRUE(web_ui()->call_data().empty());

  std::u16string query = u"www.chromium.org";
  EXPECT_CALL(
      *handler.mock_service(),
      QueryHistory(query,
                   ::testing::Field(&history::QueryOptions::host_only, true)));

  base::Value::List init_args;
  init_args.Append("query-history-callback-id");
  init_args.Append("host:www.chromium.org");
  init_args.Append(150);
  handler.HandleQueryHistory(init_args);
}

TEST_F(BrowsingHistoryHandlerTest, WithoutHostPrefixParameter) {
  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  ASSERT_TRUE(web_ui()->call_data().empty());

  std::u16string query = u"www.chromium.org";
  EXPECT_CALL(
      *handler.mock_service(),
      QueryHistory(query,
                   ::testing::Field(&history::QueryOptions::host_only, false)));

  base::Value::List init_args;
  init_args.Append("query-history-callback-id");
  init_args.Append("www.chromium.org");
  init_args.Append(150);
  handler.HandleQueryHistory(init_args);
}

TEST_F(BrowsingHistoryHandlerTest, MisplacedHostPrefixParameter) {
  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  ASSERT_TRUE(web_ui()->call_data().empty());
  {
    std::u16string query = u"whost:ww.chromium.org";
    EXPECT_CALL(
        *handler.mock_service(),
        QueryHistory(
            query, ::testing::Field(&history::QueryOptions::host_only, false)));

    base::Value::List init_args;
    init_args.Append("query-history-callback-id");
    init_args.Append("whost:ww.chromium.org");
    init_args.Append(150);
    handler.HandleQueryHistory(init_args);
  }

  {
    std::u16string query = u"www.chromium.orghost:";
    EXPECT_CALL(
        *handler.mock_service(),
        QueryHistory(
            query, ::testing::Field(&history::QueryOptions::host_only, false)));

    base::Value::List init_args;
    init_args.Append("query-history-callback-id");
    init_args.Append("www.chromium.orghost:");
    init_args.Append(150);
    handler.HandleQueryHistory(init_args);
  }
}

TEST_F(BrowsingHistoryHandlerTest, BeginTimestamp) {
  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  ASSERT_TRUE(web_ui()->call_data().empty());
  {
    std::u16string query = u"query";
    double timestamp = 1713546406359L;
    EXPECT_CALL(
        *handler.mock_service(),
        QueryHistory(
            query, ::testing::Field(
                       &history::QueryOptions::begin_time,
                       base::Time::FromMillisecondsSinceUnixEpoch(timestamp))));

    base::Value::List init_args;
    init_args.Append("query-history-callback-id");
    init_args.Append(query);
    init_args.Append(150);
    init_args.Append(timestamp);
    handler.HandleQueryHistory(init_args);
  }

  {
    std::u16string query = u"www.chromium.orghost:";
    EXPECT_CALL(
        *handler.mock_service(),
        QueryHistory(
            query, ::testing::Field(&history::QueryOptions::host_only, false)));

    base::Value::List init_args;
    init_args.Append("query-history-callback-id");
    init_args.Append("www.chromium.orghost:");
    init_args.Append(150);
    handler.HandleQueryHistory(init_args);
  }
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(BrowsingHistoryHandlerTest, MdTruncatesTitles) {
  history::BrowsingHistoryService::HistoryEntry long_url_entry;
  long_url_entry.url = GURL(
      "http://loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "ngurlislong.com");
  ASSERT_GT(long_url_entry.url.spec().size(), 300U);

  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  ASSERT_TRUE(web_ui()->call_data().empty());

  handler.OnQueryComplete({long_url_entry},
                          history::BrowsingHistoryService::QueryResultsInfo(),
                          base::OnceClosure());
  InitializeWebUI(handler);
  ASSERT_FALSE(web_ui()->call_data().empty());

  // Request should be resolved successfully.
  ASSERT_TRUE(web_ui()->call_data().front()->arg2()->GetBool());
  const base::Value* arg3 = web_ui()->call_data().front()->arg3();
  ASSERT_TRUE(arg3->is_dict());
  const base::Value* list = arg3->GetDict().Find("value");
  ASSERT_TRUE(list->is_list());

  const base::Value& first_entry = list->GetList()[0];
  ASSERT_TRUE(first_entry.is_dict());

  const std::string* title = first_entry.GetDict().FindString("title");
  ASSERT_TRUE(title);

  ASSERT_EQ(0u, title->find("http://loooo"));
  EXPECT_EQ(300u, title->size());
}

TEST_F(BrowsingHistoryHandlerTest, Reload) {
  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  handler.RegisterMessages();
  handler.PostponeResults();
  handler.StartQueryHistory();
  ASSERT_TRUE(web_ui()->call_data().empty());
  InitializeWebUI(handler);
  // Still empty, since no results are available yet.
  ASSERT_TRUE(web_ui()->call_data().empty());

  // Simulate page refresh and results being returned asynchronously.
  handler.OnJavascriptDisallowed();
  history::BrowsingHistoryService::HistoryEntry url_entry;
  url_entry.url = GURL("https://www.chromium.org");
  handler.OnQueryComplete({url_entry},
                          history::BrowsingHistoryService::QueryResultsInfo(),
                          base::OnceClosure());

  // There should be no new Web UI calls, since JS is still disallowed.
  ASSERT_TRUE(web_ui()->call_data().empty());
}
#endif
