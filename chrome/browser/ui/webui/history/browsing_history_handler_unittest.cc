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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/history/core/browser/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/data_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
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

class MockHistoryPage : public history::mojom::Page {
 public:
  MockHistoryPage() = default;
  ~MockHistoryPage() override = default;

  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::PendingRemote<history::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnHistoryDeleted, (), (override));
  MOCK_METHOD(void, OnHasOtherFormsChanged, (bool), (override));
  MOCK_METHOD(void,
              SendAccountInfo,
              (history::mojom::AccountInfoPtr),
              (override));

 private:
  mojo::Receiver<history::mojom::Page> receiver_{this};
};

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

    mock_page_ = std::make_unique<MockHistoryPage>();
    handler_->SetPage(mock_page_->BindAndGetRemote());
  }

  void MockHistoryServiceCall(
      const std::u16string& search_text,
      const QueryOptions& options,
      std::vector<BrowsingHistoryService::HistoryEntry> mock_results = {}) {
    EXPECT_CALL(
        *handler_->mock_service(),
        QueryHistory(
            search_text,
            ::testing::FieldsAre(
                /*begin_time=*/options.begin_time,
                /*end_time=*/options.end_time,
                /*max_count=*/150, history::VisitQuery404sPolicy::kExclude404s,
                /*duplicate_policy=*/
                history::QueryOptions::REMOVE_DUPLICATES_PER_DAY,
                /*matching_algorithm=*/options.matching_algorithm,
                /*host_only=*/options.host_only,
                /*visit_order=*/options.visit_order,
                /*app_id=*/options.app_id,
                /*include_actor_visits=*/true,
                /*include_user_visits=*/true,
                /*restrict_to_synced_urls=*/false,
                /*client_ids=*/testing::IsEmpty())))
        .Times(1)
        .WillOnce([&, mock_results](const std::u16string& search_text,
                                    const QueryOptions& options) {
          std::vector<BrowsingHistoryService::HistoryEntry> results;
          if (mock_results.empty()) {
            BrowsingHistoryService::HistoryEntry entry(
                BrowsingHistoryService::HistoryEntry::LOCAL_ENTRY,
                GURL(("http://test.com")), u"Test",
                base::Time::Now() - base::Minutes(5), std::string(), false,
                std::u16string(), false, GURL(), 0, 0,
                /*is_actor_visit=*/false, history::kNoAppIdFilter,
                /*visit_id=*/history::kInvalidVisitID);
            results.push_back(entry);
          }

          BrowsingHistoryService::QueryResultsInfo info;
          info.search_text = search_text;
          info.reached_beginning = true;
          info.sync_timed_out = false;
          handler_->OnQueryComplete(
              mock_results.empty() ? results : mock_results, info,
              base::OnceClosure());
        });
  }

  mojom::QueryResultPtr RunQueryHistory(
      const std::string& query,
      std::optional<double> begin_timestamp = std::nullopt) {
    mojom::QueryResultPtr history_query_results;
    base::RunLoop run_loop;
    handler_->QueryHistory(
        query, 150, begin_timestamp, true, true,
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
    mock_page_.reset();
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
  MockHistoryPage* mock_page() { return mock_page_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<BrowsingHistoryHandlerWithWebUIForTesting> handler_;
  std::unique_ptr<MockHistoryPage> mock_page_;
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

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(BrowsingHistoryHandlerTest, RequestAccountInfo) {
  // Check that the account info is sent to the page.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@example.com", signin::ConsentLevel::kSignin);
  account_info =
      AccountInfo::Builder(account_info).SetFullName("Test User").Build();
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  base::MockCallback<BrowsingHistoryHandler::RequestAccountInfoCallback>
      callback;
  history::mojom::AccountInfoPtr account_info_ptr;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&](history::mojom::AccountInfoPtr ptr) {
        account_info_ptr = std::move(ptr);
      });

  handler()->RequestAccountInfo(callback.Get());

  ASSERT_TRUE(account_info_ptr);
  EXPECT_EQ("test@example.com", account_info_ptr->email);
  EXPECT_EQ("Test User", account_info_ptr->name);
}

TEST_F(BrowsingHistoryHandlerTest, TurnOnHistorySync) {
  // This test doesn't create a Browser instance, so FindBrowserWithTab
  // returns nullptr. TurnOnHistorySync should handle this without crashing.
  handler()->TurnOnHistorySync();
}

TEST_F(BrowsingHistoryHandlerTest, ObservesIdentityManagerOnlyAfterRequest) {
  // Check that the identity manager is only observed after RequestAccountInfo
  // is called.
  ASSERT_FALSE(handler()->is_observing_identity_manager_for_testing());
  handler()->RequestAccountInfo(base::DoNothing());
  EXPECT_TRUE(handler()->is_observing_identity_manager_for_testing());
}

TEST_F(BrowsingHistoryHandlerTest, SendsUpdatedInfoOnAccountChange) {
  // Check that the account info is sent to the page when it is updated.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@example.com", signin::ConsentLevel::kSignin);

  base::MockCallback<BrowsingHistoryHandler::RequestAccountInfoCallback>
      callback;
  EXPECT_CALL(callback, Run(::testing::_));
  handler()->RequestAccountInfo(callback.Get());

  EXPECT_CALL(*mock_page(), SendAccountInfo(_));
  // Update the account info with all the necessary fields for
  // AccountInfo::isValid() to be true.
  account_info = AccountInfo::Builder(account_info)
                     .SetFullName("Test User")
                     .SetGivenName("Test")
                     .SetHostedDomain("example.com")
                     .SetAvatarUrl("http://example.com/test.jpg")
                     .Build();
  ASSERT_TRUE(account_info.IsValid());

  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  mock_page()->FlushForTesting();
}

TEST_F(BrowsingHistoryHandlerTest, IncludeActorVisits) {
  std::u16string query = u"test";
  QueryOptions options;
  options.include_actor_visits = true;

  MockHistoryServiceCall(query, options);
  RunQueryHistory("test");
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(BrowsingHistoryHandlerTest, QueryHistoryMojoOptionMapping) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      history::kBrowsingHistoryActorIntegrationM3);

  EXPECT_CALL(
      *handler()->mock_service(),
      QueryHistory(
          testing::Eq(u"query"),
          testing::Field(&history::QueryOptions::include_user_visits, false)))
      .Times(1);

  handler()->QueryHistory("query", 150, std::nullopt,
                          /*include_user_visits=*/false,
                          /*include_actor_visits=*/true, base::DoNothing());
}
#endif

TEST_F(BrowsingHistoryHandlerTest, QueryHistoryWithActorOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      history::kBrowsingHistoryActorIntegrationM3);

  EXPECT_CALL(*handler()->mock_service(),
              QueryHistory(
                  testing::Eq(u"test_query"),
                  testing::AllOf(
                      testing::Field(
                          &history::QueryOptions::include_user_visits, false),
                      testing::Field(
                          &history::QueryOptions::include_actor_visits, true))))
      .Times(1);

  handler()->QueryHistory("test_query", 150, std::nullopt,
                          /*include_user_visits=*/false,
                          /*include_actor_visits=*/true, base::DoNothing());
}

TEST_F(BrowsingHistoryHandlerTest, QueryHistoryWithUserOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      history::kBrowsingHistoryActorIntegrationM3);

  EXPECT_CALL(
      *handler()->mock_service(),
      QueryHistory(
          testing::Eq(u"test_query"),
          testing::AllOf(
              testing::Field(&history::QueryOptions::include_user_visits, true),
              testing::Field(&history::QueryOptions::include_actor_visits,
                             false))))
      .Times(1);

  handler()->QueryHistory("test_query", 150, std::nullopt,
                          /*include_user_visits=*/true,
                          /*include_actor_visits=*/false, base::DoNothing());
}

TEST_F(BrowsingHistoryHandlerTest, QueryHistoryWithBothVisits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      history::kBrowsingHistoryActorIntegrationM3);

  EXPECT_CALL(
      *handler()->mock_service(),
      QueryHistory(
          testing::Eq(u"test_query"),
          testing::AllOf(
              testing::Field(&history::QueryOptions::include_user_visits, true),
              testing::Field(&history::QueryOptions::include_actor_visits,
                             true))))
      .Times(1);

  handler()->QueryHistory("test_query", 150, std::nullopt,
                          /*include_user_visits=*/true,
                          /*include_actor_visits=*/true, base::DoNothing());
}

class BrowsingHistoryHandlerHistorySyncPromoTest
    : public BrowsingHistoryHandlerTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    BrowsingHistoryHandlerTest::SetUp();
    SetUpIdentityState();
  }

 private:
  // Sets up the identity state based on the test parameter.
  // If the parameter is `true`, a primary account is set up. Otherwise, the
  // profile is left signed out.
  void SetUpIdentityState() {
    if (GetParam()) {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile());
      signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                          signin::ConsentLevel::kSignin);
    }
  }
};

TEST_P(BrowsingHistoryHandlerHistorySyncPromoTest,
       ShouldShowHistoryPageHistorySyncPromoShownLessThanThreshold) {
  for (int i = 0; i < 4; ++i) {
    handler()->IncrementHistoryPageHistorySyncPromoShownCount();
  }
  base::MockCallback<
      BrowsingHistoryHandler::ShouldShowHistoryPageHistorySyncPromoCallback>
      callback;
  EXPECT_CALL(callback, Run(true));
  handler()->ShouldShowHistoryPageHistorySyncPromo(callback.Get());
}

TEST_P(BrowsingHistoryHandlerHistorySyncPromoTest,
       ShouldShowHistoryPageHistorySyncPromoShownEqualToThreshold) {
  for (int i = 0; i < 5; ++i) {
    handler()->IncrementHistoryPageHistorySyncPromoShownCount();
  }
  base::MockCallback<
      BrowsingHistoryHandler::ShouldShowHistoryPageHistorySyncPromoCallback>
      callback;
  EXPECT_CALL(callback, Run(false));
  handler()->ShouldShowHistoryPageHistorySyncPromo(callback.Get());
}

TEST_P(BrowsingHistoryHandlerHistorySyncPromoTest,
       ShouldShowHistoryPageHistorySyncPromoDismissedOnceCooldownPassed) {
  handler()->RecordHistoryPageHistorySyncPromoDismissed();
  handler()->test_clock()->Advance(base::Days(8));
  base::MockCallback<
      BrowsingHistoryHandler::ShouldShowHistoryPageHistorySyncPromoCallback>
      callback;
  EXPECT_CALL(callback, Run(true));
  handler()->ShouldShowHistoryPageHistorySyncPromo(callback.Get());
}

TEST_P(BrowsingHistoryHandlerHistorySyncPromoTest,
       ShouldShowHistoryPageHistorySyncPromoDismissedOnceCooldownNotPassed) {
  handler()->RecordHistoryPageHistorySyncPromoDismissed();
  handler()->test_clock()->Advance(base::Days(6));
  base::MockCallback<
      BrowsingHistoryHandler::ShouldShowHistoryPageHistorySyncPromoCallback>
      callback;
  EXPECT_CALL(callback, Run(false));
  handler()->ShouldShowHistoryPageHistorySyncPromo(callback.Get());
}

TEST_P(BrowsingHistoryHandlerHistorySyncPromoTest,
       ShouldShowHistoryPageHistorySyncPromoShownAfterDismissal) {
  handler()->RecordHistoryPageHistorySyncPromoDismissed();
  handler()->test_clock()->Advance(base::Days(8));
  base::MockCallback<
      BrowsingHistoryHandler::ShouldShowHistoryPageHistorySyncPromoCallback>
      callback_before_shown_after_dismissal;
  EXPECT_CALL(callback_before_shown_after_dismissal, Run(true));
  handler()->ShouldShowHistoryPageHistorySyncPromo(
      callback_before_shown_after_dismissal.Get());

  handler()->IncrementHistoryPageHistorySyncPromoShownCount();

  base::MockCallback<
      BrowsingHistoryHandler::ShouldShowHistoryPageHistorySyncPromoCallback>
      callback_shown_after_dismissal;
  EXPECT_CALL(callback_shown_after_dismissal, Run(false));
  handler()->ShouldShowHistoryPageHistorySyncPromo(
      callback_shown_after_dismissal.Get());
}

INSTANTIATE_TEST_SUITE_P(,
                         BrowsingHistoryHandlerHistorySyncPromoTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SignedIn" : "SignedOut";
                         });

class BrowsingHistoryHandlerCriticalActionsTest
    : public BrowsingHistoryHandlerTest {
 protected:
  void SetUp() override {
    BrowsingHistoryHandlerTest::SetUp();
    feature_list_.InitAndEnableFeature(
        critical_actions::features::kCriticalActionHistory);
    critical_action_service_ =
        critical_actions::CriticalActionFactory::GetForProfile(profile());
    ASSERT_NE(critical_action_service_, nullptr);
  }

  void TearDown() override {
    critical_action_service_ = nullptr;
    BrowsingHistoryHandlerTest::TearDown();
  }

  critical_actions::CriticalActionService* service() {
    return critical_action_service_;
  }

  critical_actions::CriticalActionEntry CreateAction(
      std::string_view id,
      base::Time timestamp,
      int64_t visit_id,
      critical_actions::ActionType type =
          critical_actions::ActionType::kFormFill,
      std::string_view conversation_id = "",
      std::string_view actor_task_id = "",
      std::string_view metadata = "") {
    critical_actions::CriticalActionEntry entry;
    entry.critical_action_id = std::string(id);
    entry.timestamp = timestamp;
    entry.visit_id = visit_id;
    entry.action_type = type;
    entry.url = GURL("http://actor-example.com");
    entry.conversation_id = std::string(conversation_id);
    entry.actor_task_id = std::string(actor_task_id);
    entry.metadata = std::string(metadata);
    return entry;
  }

  BrowsingHistoryService::HistoryEntry CreateActorHistoryEntry(
      base::Time timestamp,
      int64_t visit_id,
      const std::u16string& title = u"Actor Visit",
      int visit_count = 1) {
    return BrowsingHistoryService::HistoryEntry(
        BrowsingHistoryService::HistoryEntry::LOCAL_ENTRY,
        GURL("http://actor-example.com"), title, timestamp, std::string(),
        false, std::u16string(), false, GURL(), visit_count, 0,
        /*is_actor_visit=*/true, history::kNoAppIdFilter, visit_id);
  }

  void FlushDatabaseTasks() {
    base::test::TestFuture<std::vector<critical_actions::CriticalActionEntry>>
        future;
    critical_actions::CriticalActionQueryOptions options;
    service()->GetCriticalActions(options, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<critical_actions::CriticalActionService> critical_action_service_ =
      nullptr;
};

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       CriticalActionsPopulatedForActorVisits) {
  base::HistogramTester histogram_tester;
  base::Time visit_time = base::Time::Now();

  service()->AddCriticalAction(
      CreateAction("test-action-id-1", visit_time, 42,
                   critical_actions::ActionType::kCredentialAccess));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  EXPECT_TRUE(results->value[0]->is_actor_visit);
  ASSERT_EQ(results->value[0]->critical_actions.size(), 1u);
  EXPECT_EQ(results->value[0]->critical_actions[0]->id, "test-action-id-1");
  EXPECT_EQ(
      results->value[0]->critical_actions[0]->label,
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PASSWORD_FILLED));
  EXPECT_EQ(
      results->value[0]->critical_actions[0]->tooltip,
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PASSWORD_TOOLTIP));
  EXPECT_EQ(results->value[0]->critical_actions[0]->linkout_url,
            "chrome://password-manager/passwords/actor-example.com");
  EXPECT_EQ(
      results->value[0]->critical_actions[0]->action_type,
      history::mojom::CriticalActionType::kCredentialAccess);

  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 1);
  histogram_tester.ExpectTotalCount(
      "HistoryPage.QueryHistoryTotalTime.WithCriticalActions", 1);
  histogram_tester.ExpectTotalCount(
      "HistoryPage.QueryHistoryTotalTime.WithoutCriticalActions", 0);
  histogram_tester.ExpectUniqueSample(
      "HistoryPage.CriticalActionsPerVisitCount", 1, 1);
}

TEST_F(BrowsingHistoryHandlerTest,
       QueryPerformanceMetricsEmittedWithoutCriticalActions) {
  base::HistogramTester histogram_tester;
  base::Time visit_time = base::Time::Now();

  BrowsingHistoryService::HistoryEntry non_actor_entry(
      BrowsingHistoryService::HistoryEntry::LOCAL_ENTRY,
      GURL("http://regular-example.com"), u"Regular Visit", visit_time,
      std::string(), false, std::u16string(), false, GURL(), 1, 0,
      /*is_actor_visit=*/false, history::kNoAppIdFilter, /*visit_id=*/10);

  QueryOptions options;
  MockHistoryServiceCall(u"regular-example", options, {non_actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("regular-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  EXPECT_FALSE(results->value[0]->is_actor_visit);

  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsQueryTime", 0);
  histogram_tester.ExpectTotalCount(
      "HistoryPage.QueryHistoryTotalTime.WithCriticalActions", 0);
  histogram_tester.ExpectTotalCount(
      "HistoryPage.QueryHistoryTotalTime.WithoutCriticalActions", 1);
  histogram_tester.ExpectTotalCount("HistoryPage.CriticalActionsPerVisitCount",
                                    0);
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       CriticalActionsDistinguishesSeparateVisitsSameUrl) {
  base::Time visit_time1 = base::Time::Now() - base::Minutes(20);
  base::Time visit_time2 = base::Time::Now();

  service()->AddCriticalAction(
      CreateAction("action-visit-1", visit_time1 + base::Seconds(5), 1001,
                   critical_actions::ActionType::kCredentialAccess));
  service()->AddCriticalAction(
      CreateAction("action-visit-2", visit_time2 + base::Seconds(5), 1002,
                   critical_actions::ActionType::kDownload));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry entry2 =
      CreateActorHistoryEntry(visit_time2, 1002, u"Visit 2");
  BrowsingHistoryService::HistoryEntry entry1 =
      CreateActorHistoryEntry(visit_time1, 1001, u"Visit 1");

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {entry2, entry1});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 2u);

  // Entry 2 (visit 1002) has action "download".
  ASSERT_EQ(results->value[0]->critical_actions.size(), 1u);
  EXPECT_EQ(results->value[0]->critical_actions[0]->id, "action-visit-2");
  EXPECT_EQ(results->value[0]->critical_actions[0]->label,
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_DOWNLOAD));

  // Entry 1 (visit 1001) has action "password".
  ASSERT_EQ(results->value[1]->critical_actions.size(), 1u);
  EXPECT_EQ(results->value[1]->critical_actions[0]->id, "action-visit-1");
  EXPECT_EQ(
      results->value[1]->critical_actions[0]->label,
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PASSWORD_FILLED));
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       CriticalActionsAttachedToAggregatedActorVisits) {
  base::test::ScopedFeatureList local_features(
      history::kBrowsingHistoryActorIntegrationM3);

  base::Time visit_time1 = base::Time::Now() - base::Minutes(20);
  base::Time visit_time2 = base::Time::Now();

  service()->AddCriticalAction(
      CreateAction("action-visit-1", visit_time1 + base::Seconds(5), 1001,
                   critical_actions::ActionType::kCredentialAccess));
  service()->AddCriticalAction(
      CreateAction("action-visit-2", visit_time2 + base::Seconds(5), 1002,
                   critical_actions::ActionType::kDownload));
  FlushDatabaseTasks();

  // Single aggregated entry representing both visits on the same day.
  BrowsingHistoryService::HistoryEntry entry =
      CreateActorHistoryEntry(visit_time2, 1002, u"Visit", /*visit_count=*/2);
  entry.all_visit_ids.push_back(1001);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  ASSERT_EQ(results->value[0]->critical_actions.size(), 2u);
  EXPECT_EQ(results->value[0]->critical_actions[0]->id, "action-visit-2");
  EXPECT_EQ(results->value[0]->critical_actions[1]->id, "action-visit-1");
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       UnmatchedActorFormFillsAreShownAsIs) {
  base::Time visit_time = base::Time::Now();

  service()->AddCriticalAction(
      CreateAction("test-action-id-actor", visit_time, 42,
                   critical_actions::ActionType::kFormFill, "test-conv",
                   "test-task", "{\"requested_data\": [\"FIRST_NAME\"]}"));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  EXPECT_TRUE(results->value[0]->is_actor_visit);
  // The critical action should be shown because unmatched actions are shown
  // as-is.
  ASSERT_EQ(results->value[0]->critical_actions.size(), 1u);
  EXPECT_EQ(results->value[0]->critical_actions[0]->id, "test-action-id-actor");
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest, MatchedFormFillsAreMerged) {
  base::Time visit_time = base::Time::Now();

  // 1. Add actor action.
  service()->AddCriticalAction(CreateAction(
      "action-actor", visit_time, 42, critical_actions::ActionType::kFormFill,
      "test-conv", "test-task", "{\"requested_data\": [\"FIRST_NAME\"]}"));

  // 2. Add system action with matching properties and timestamp within 5
  // seconds.
  service()->AddCriticalAction(
      CreateAction("action-system", visit_time + base::Seconds(2), 42,
                   critical_actions::ActionType::kFormFill, "test-conv",
                   "test-task", "{\"filled_types\": [\"FIRST_NAME\"]}"));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  // Merged into 1 action in WebUI.
  ASSERT_EQ(results->value[0]->critical_actions.size(), 1u);
  EXPECT_EQ(results->value[0]->critical_actions[0]->id, "action-system");
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       UnmatchedSystemFormFillsAreShownAsIs) {
  base::Time visit_time = base::Time::Now();

  // Add system action only.
  service()->AddCriticalAction(
      CreateAction("action-system-only", visit_time, 42,
                   critical_actions::ActionType::kFormFill, "test-conv",
                   "test-task", "{\"filled_types\": [\"FIRST_NAME\"]}"));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  // Unmatched system action is displayed.
  ASSERT_EQ(results->value[0]->critical_actions.size(), 1u);
  EXPECT_EQ(results->value[0]->critical_actions[0]->id, "action-system-only");
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       MatchedFormFillsOutsideToleranceWindowAreNotMerged) {
  base::Time visit_time = base::Time::Now();

  // 1. Add actor action.
  service()->AddCriticalAction(CreateAction(
      "action-actor", visit_time, 42, critical_actions::ActionType::kFormFill,
      "test-conv", "test-task", "{\"requested_data\": [\"FIRST_NAME\"]}"));

  // 2. Add system action with matching properties but timestamp outside 5
  // seconds (e.g. 6 seconds later).
  service()->AddCriticalAction(
      CreateAction("action-system", visit_time + base::Seconds(6), 42,
                   critical_actions::ActionType::kFormFill, "test-conv",
                   "test-task", "{\"filled_types\": [\"FIRST_NAME\"]}"));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  // Not merged. Both actions are displayed.
  ASSERT_EQ(results->value[0]->critical_actions.size(), 2u);
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       DifferentActionTypesWithinToleranceWindowAreNotMerged) {
  base::Time visit_time = base::Time::Now();

  // 1. Add actor action (FormFill).
  service()->AddCriticalAction(CreateAction(
      "action-actor", visit_time, 42, critical_actions::ActionType::kFormFill,
      "test-conv", "test-task", "{\"requested_data\": [\"FIRST_NAME\"]}"));

  // 2. Add system action with matching timestamp but different action type
  // (CredentialsOtp) within 5 seconds.
  service()->AddCriticalAction(
      CreateAction("action-system", visit_time + base::Seconds(2), 42,
                   critical_actions::ActionType::kCredentialsOtp, "test-conv",
                   "test-task", "{\"filled_types\": [\"FIRST_NAME\"]}"));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  // Not merged. Both actions are displayed.
  ASSERT_EQ(results->value[0]->critical_actions.size(), 2u);
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       DifferentConversationsWithinToleranceWindowAreNotMerged) {
  base::Time visit_time = base::Time::Now();

  // 1. Add actor action (conv 1).
  service()->AddCriticalAction(CreateAction(
      "action-actor", visit_time, 42, critical_actions::ActionType::kFormFill,
      "test-conv-1", "test-task", "{\"requested_data\": [\"FIRST_NAME\"]}"));

  // 2. Add system action within 5 seconds but different conversation ID (conv
  // 2).
  service()->AddCriticalAction(
      CreateAction("action-system", visit_time + base::Seconds(2), 42,
                   critical_actions::ActionType::kFormFill, "test-conv-2",
                   "test-task", "{\"filled_types\": [\"FIRST_NAME\"]}"));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  // Not merged. Both actions are displayed.
  ASSERT_EQ(results->value[0]->critical_actions.size(), 2u);
}

TEST_F(BrowsingHistoryHandlerCriticalActionsTest,
       MatchedFormFillsBoundaryTest) {
  base::Time visit_time = base::Time::Now();

  // We add three actions:
  // 1. System action at visit_time.
  // 2. System action at exactly visit_time - 5 seconds (on boundary).
  // 3. System action at visit_time - 5 seconds - 1 millisecond (outside
  // boundary).
  service()->AddCriticalAction(CreateAction(
      "action-1", visit_time, 42, critical_actions::ActionType::kFormFill,
      "test-conv", "test-task", "{\"filled_types\": [\"FIRST_NAME\"]}"));
  service()->AddCriticalAction(
      CreateAction("action-2", visit_time - base::Seconds(5), 42,
                   critical_actions::ActionType::kFormFill, "test-conv",
                   "test-task", "{\"filled_types\": [\"FIRST_NAME\"]}"));
  service()->AddCriticalAction(CreateAction(
      "action-3", visit_time - base::Seconds(5) - base::Milliseconds(1), 42,
      critical_actions::ActionType::kFormFill, "test-conv", "test-task",
      "{\"filled_types\": [\"FIRST_NAME\"]}"));
  FlushDatabaseTasks();

  BrowsingHistoryService::HistoryEntry actor_entry =
      CreateActorHistoryEntry(visit_time, 42);

  QueryOptions options;
  MockHistoryServiceCall(u"actor-example", options, {actor_entry});

  mojom::QueryResultPtr results = RunQueryHistory("actor-example");
  ASSERT_TRUE(results);
  ASSERT_EQ(results->value.size(), 1u);
  // action-1 is accepted.
  // action-2 is on boundary (exactly 5s delta), so it's merged (dropped).
  // action-3 is outside boundary (> 5s delta), so it is accepted.
  // Total accepted actions = 2 (action-1 and action-3).
  ASSERT_EQ(results->value[0]->critical_actions.size(), 2u);
  EXPECT_EQ(results->value[0]->critical_actions[0]->id, "action-1");
  EXPECT_EQ(results->value[0]->critical_actions[1]->id, "action-3");
}

TEST_F(BrowsingHistoryHandlerTest,
       OpenCriticalActionConversationInvalidAction) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {critical_actions::features::kCriticalActionHistory,
       history::kBrowsingHistoryActorIntegrationM3},
      {});

  base::HistogramTester histogram_tester;
  base::test::TestFuture<history::mojom::OpenConversationResult> future;
  handler()->OpenCriticalActionConversation("non-existent-action",
                                            future.GetCallback());
  EXPECT_EQ(future.Get(),
            history::mojom::OpenConversationResult::kErrorInvalidActionEntry);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult",
      critical_actions::OpenConversationResult::kErrorInvalidActionEntry, 1);
}

TEST_F(BrowsingHistoryHandlerTest,
       OpenCriticalActionConversationCallsLinkoutHandler) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {critical_actions::features::kCriticalActionHistory,
       history::kBrowsingHistoryActorIntegrationM3},
      {});

  critical_actions::CriticalActionService* critical_action_service =
      critical_actions::CriticalActionFactory::GetForProfile(profile());
  ASSERT_NE(critical_action_service, nullptr);

  critical_actions::CriticalActionEntry action;
  action.critical_action_id = "test-action-1";
  action.conversation_id = "c_test_conversation";
  action.timestamp = base::Time::Now();
  action.visit_id = 1001;
  action.action_type = critical_actions::ActionType::kDownload;
  action.url = GURL("http://example.com");
  critical_action_service->AddCriticalAction(action);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<history::mojom::OpenConversationResult> future;
  handler()->OpenCriticalActionConversation("test-action-1",
                                            future.GetCallback());
  EXPECT_EQ(future.Get(),
            history::mojom::OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult",
      critical_actions::OpenConversationResult::kErrorInternal, 1);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

class BrowsingHistoryHandlerHatsSurveyTest
    : public BrowsingHistoryHandlerTest,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, const char*, const base::Feature*>> {
 public:
  BrowsingHistoryHandlerHatsSurveyTest() {
    std::tie(history_page_improvements_enabled_, hats_feature_enabled_,
             should_launch_, hats_trigger_, hats_feature_) = GetParam();

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (history_page_improvements_enabled_) {
      enabled_features.push_back(history::kBrowsingHistoryActorIntegrationM3);
      enabled_features.push_back(
          history::kBrowsingHistorySimilarVisitsGrouping);
    } else {
      disabled_features.push_back(history::kBrowsingHistoryActorIntegrationM3);
      disabled_features.push_back(
          history::kBrowsingHistorySimilarVisitsGrouping);
    }

    if (hats_feature_enabled_ && hats_feature_) {
      enabled_features.push_back(*hats_feature_);
    } else if (hats_feature_) {
      disabled_features.push_back(*hats_feature_);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    BrowsingHistoryHandlerTest::SetUp();
    HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&BuildMockHatsService));
  }

 protected:
  bool history_page_improvements_enabled_;
  bool hats_feature_enabled_;
  bool should_launch_;
  const char* hats_trigger_;
  raw_ptr<const base::Feature> hats_feature_ = nullptr;
  base::test::ScopedFeatureList feature_list_;

 private:
  static std::unique_ptr<KeyedService> BuildMockHatsService(
      content::BrowserContext* context) {
    return std::make_unique<testing::StrictMock<MockHatsService>>(
        Profile::FromBrowserContext(context));
  }
};

TEST_P(BrowsingHistoryHandlerHatsSurveyTest, TriggersHatsSurvey) {
  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->GetForProfile(
          profile(),
          /*create_if_necessary=*/true));
  if (should_launch_) {
    EXPECT_CALL(
        *mock_hats_service,
        LaunchDelayedSurveyForWebContents(
            hats_trigger_, web_contents(), testing::_, testing::_, testing::_,
            testing::_, testing::_, testing::_, testing::_, testing::_));
  } else {
    EXPECT_CALL(*mock_hats_service,
                LaunchDelayedSurveyForWebContents(_, _, _, _, _, _, _, _, _, _))
        .Times(0);
  }

  // Re-create handler to pick up feature flags.
  auto handler = std::make_unique<BrowsingHistoryHandlerWithWebUIForTesting>(
      mojo::PendingReceiver<history::mojom::PageHandler>(), profile(),
      web_contents());
  auto mock_page = std::make_unique<MockHistoryPage>();
  handler->SetPage(mock_page->BindAndGetRemote());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowsingHistoryHandlerHatsSurveyTest,
    testing::Values(
        std::make_tuple(
            /*history_page_improvements_enabled_=*/true,
            /*hats_feature_enabled_=*/true,
            /*should_launch_=*/true,
            /*hats_trigger_=*/kHatsSurveyTriggerHistoryPageExperiment,
            /*hats_feature_=*/
            &features::
                kHappinessTrackingSurveysForDesktopHistoryPageExperiment),
        std::make_tuple(
            /*history_page_improvements_enabled_=*/true,
            /*hats_feature_enabled_=*/false,
            /*should_launch_=*/false,
            /*hats_trigger_=*/kHatsSurveyTriggerHistoryPageExperiment,
            /*hats_feature_=*/
            &features::
                kHappinessTrackingSurveysForDesktopHistoryPageExperiment),
        std::make_tuple(
            /*history_page_improvements_enabled_=*/false,
            /*hats_feature_enabled_=*/true,
            /*should_launch_=*/true,
            /*hats_trigger_=*/kHatsSurveyTriggerHistoryPageControl,
            /*hats_feature_=*/
            &features::kHappinessTrackingSurveysForDesktopHistoryPageControl),
        std::make_tuple(
            /*history_page_improvements_enabled_=*/false,
            /*hats_feature_enabled_=*/false,
            /*should_launch_=*/false,
            /*hats_trigger_=*/kHatsSurveyTriggerHistoryPageControl,
            /*hats_feature_=*/
            &features::kHappinessTrackingSurveysForDesktopHistoryPageControl),
        // No trigger should be launched if the history actor state is swapped.
        std::make_tuple(
            /*history_page_improvements_enabled_=*/false,
            /*hats_feature_enabled_=*/true,
            /*should_launch_=*/false,
            /*hats_trigger_=*/kHatsSurveyTriggerHistoryPageExperiment,
            /*hats_feature_=*/
            &features::
                kHappinessTrackingSurveysForDesktopHistoryPageExperiment),
        std::make_tuple(
            /*history_page_improvements_enabled_=*/true,
            /*hats_feature_enabled_=*/true,
            /*should_launch_=*/false,
            /*hats_trigger_=*/kHatsSurveyTriggerHistoryPageControl,
            /*hats_feature_=*/
            &features::kHappinessTrackingSurveysForDesktopHistoryPageControl)));
}  // namespace history
