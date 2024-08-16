// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSampleSearchUrl[] = "https://www.google.com/search?q=sample";
constexpr char kSampleSearchUrl2[] = "https://www.google.com/search?q=sample2";

std::unique_ptr<sync_sessions::SyncedSession> BuildSampleSession(
    const char* session_name,
    std::vector<std::unique_ptr<sync_sessions::SyncedSessionWindow>>
        session_windows,
    base::Time modified_time = base::Time::Now()) {
  auto sample_session = std::make_unique<sync_sessions::SyncedSession>();
  sample_session->SetSessionName(session_name);
  sample_session->SetModifiedTime(modified_time);
  for (size_t i = 0; i < session_windows.size(); i++) {
    sample_session->windows[SessionID::FromSerializedValue(i)] =
        std::move(session_windows[i]);
  }

  return sample_session;
}

std::unique_ptr<sync_sessions::SyncedSessionWindow> SampleSessionWindow(
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs,
    base::Time time = base::Time::Now()) {
  auto synced_session_window =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  synced_session_window->wrapped_window.timestamp = time;
  synced_session_window->wrapped_window.tabs = std::move(tabs);
  return synced_session_window;
}

std::unique_ptr<sessions::SessionTab> SampleSessionTab(
    int tab_id,
    std::u16string title,
    GURL url,
    bool pinned,
    base::Time timestamp = base::Time::Now()) {
  sessions::SerializedNavigationEntry navigation;
  navigation.set_title(title);
  navigation.set_virtual_url(url);
  navigation.set_timestamp(timestamp);
  navigation.set_favicon_url(url);

  auto session_tab = std::make_unique<sessions::SessionTab>();
  session_tab->tab_id = SessionID::FromSerializedValue(tab_id);
  session_tab->current_navigation_index = 0;
  session_tab->navigations.push_back(navigation);
  session_tab->timestamp = timestamp;
  session_tab->pinned = pinned;
  return session_tab;
}

std::unique_ptr<sync_sessions::SyncedSession> GetSampleSession() {
  auto now = base::Time::Now();
  std::vector<std::unique_ptr<sessions::SessionTab>> session_window_tabs = {};
  session_window_tabs.emplace_back(SampleSessionTab(
      1, u"Tab 1", GURL(kSampleSearchUrl), true, now - base::Hours(1)));
  session_window_tabs.emplace_back(SampleSessionTab(
      2, u"Tab 2", GURL(kSampleSearchUrl2), false, now - base::Hours(2)));
  std::vector<std::unique_ptr<sync_sessions::SyncedSessionWindow>>
      session_windows = {};
  session_windows.emplace_back(
      SampleSessionWindow(std::move(session_window_tabs)));
  return BuildSampleSession("Sample Session", std::move(session_windows));
}

}  // namespace

namespace sync_sessions {

class MockOpenTabsUIDelegate : public OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate() = default;
  MockOpenTabsUIDelegate(const MockOpenTabsUIDelegate&) = delete;
  MockOpenTabsUIDelegate& operator=(const MockOpenTabsUIDelegate&) = delete;
  ~MockOpenTabsUIDelegate() override = default;

  MOCK_METHOD1(
      GetAllForeignSessions,
      bool(std::vector<raw_ptr<const SyncedSession, VectorExperimental>>*
               sessions));

  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));

  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));

  MOCK_METHOD1(
      GetForeignSession,
      std::vector<const sessions::SessionWindow*>(const std::string& tag));

  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));

  MOCK_METHOD1(GetLocalSession, bool(const SyncedSession** local_session));
};

class MockSessionSyncService : public SessionSyncService {
 public:
  MockSessionSyncService() = default;
  MockSessionSyncService(const MockSessionSyncService&) = delete;
  MockSessionSyncService& operator=(const MockSessionSyncService&) = delete;
  ~MockSessionSyncService() override = default;

  MOCK_CONST_METHOD0(GetGlobalIdMapper, syncer::GlobalIdMapper*());

  MOCK_METHOD0(GetOpenTabsUIDelegate, OpenTabsUIDelegate*());

  base::CallbackListSubscription SubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) override {
    return subscriber_list_.Add(cb);
  }

  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<syncer::DataTypeControllerDelegate>());

 private:
  base::RepeatingClosureList subscriber_list_;
};

}  // namespace sync_sessions

namespace visited_url_ranking {

using Source = URLVisit::Source;
using URLType = visited_url_ranking::FetchOptions::URLType;
using ResultOption = visited_url_ranking::FetchOptions::ResultOption;

class SessionURLVisitDataFetcherTest
    : public testing::Test,
      public ::testing::WithParamInterface<Source> {
 public:
  SessionURLVisitDataFetcherTest() = default;

  void SetSessionSyncServiceExpectations() {
    EXPECT_CALL(mock_session_sync_service_, GetOpenTabsUIDelegate())
        .WillOnce(
            testing::Invoke([this]() { return &open_tabs_ui_delegate_; }));
  }

  FetchResult FetchAndGetResult(const FetchOptions& options) {
    FetchResult result = FetchResult(FetchResult::Status::kError, {});
    base::RunLoop wait_loop;
    auto session_url_visit_data_fetcher =
        SessionURLVisitDataFetcher(&mock_session_sync_service_);
    session_url_visit_data_fetcher.FetchURLVisitData(
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
  sync_sessions::MockOpenTabsUIDelegate open_tabs_ui_delegate_;
  sync_sessions::MockSessionSyncService mock_session_sync_service_;

 private:
  base::test::TaskEnvironment task_env_;
};

TEST_F(SessionURLVisitDataFetcherTest, FetchURLVisitDataNoOpenTabsUIDelegate) {
  EXPECT_CALL(mock_session_sync_service_, GetOpenTabsUIDelegate())
      .WillOnce(testing::Invoke([]() { return nullptr; }));

  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> sample_sessions;
  sample_sessions.push_back(GetSampleSession());

  base::Time yesterday = base::Time::Now() - base::Days(1);
  auto result = FetchAndGetResult(FetchOptions(
      {{URLType::kActiveRemoteTab, {.age_limit = base::Days(1)}}},
      {
          {Fetcher::kSession,
           FetchOptions::FetchSources({URLVisit::Source::kForeign})},
      },
      yesterday));
  EXPECT_EQ(result.status, FetchResult::Status::kError);
}

TEST_F(SessionURLVisitDataFetcherTest, FetchURLVisitDataDefaultSources) {
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> sample_sessions;
  sample_sessions.push_back(GetSampleSession());

  SetSessionSyncServiceExpectations();
  EXPECT_CALL(open_tabs_ui_delegate_, GetLocalSession(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_sessions](const sync_sessions::SyncedSession** local) {
            *local = sample_sessions[0].get();
            return true;
          }));
  EXPECT_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_sessions](
              std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                  VectorExperimental>>* sessions) {
            for (auto& sample_session : sample_sessions) {
              sessions->push_back(sample_session.get());
            }
            return true;
          }));

  base::Time yesterday = base::Time::Now() - base::Days(1);
  auto result = FetchAndGetResult(FetchOptions(
      {
          {URLType::kActiveRemoteTab, {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kSession, FetchOptions::kOriginSources},
      },
      yesterday));
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 2u);
  for (const auto& url_key : {kSampleSearchUrl, kSampleSearchUrl2}) {
    const auto& tab_data =
        std::get<URLVisitAggregate::TabData>(result.data.at(url_key));
    EXPECT_EQ(tab_data.tab_count, 2u);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SessionURLVisitDataFetcherTest,
                         ::testing::Values(Source::kLocal, Source::kForeign));

TEST_P(SessionURLVisitDataFetcherTest, FetchURLVisitData) {
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> sample_sessions;
  sample_sessions.push_back(GetSampleSession());

  SetSessionSyncServiceExpectations();
  const auto source = GetParam();
  if (source == Source::kLocal) {
    EXPECT_CALL(open_tabs_ui_delegate_, GetLocalSession(testing::_))
        .WillOnce(testing::Invoke(
            [&sample_sessions](const sync_sessions::SyncedSession** local) {
              *local = sample_sessions[0].get();
              return true;
            }));
  } else if (source == Source::kForeign) {
    EXPECT_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(testing::_))
        .WillOnce(testing::Invoke(
            [&sample_sessions](
                std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                    VectorExperimental>>* sessions) {
              for (auto& sample_session : sample_sessions) {
                sessions->push_back(sample_session.get());
              }
              return true;
            }));
  }

  auto options = FetchOptions(
      {
          {URLType::kActiveRemoteTab, {.age_limit = base::Days(1)}},
      },
      {
          {Fetcher::kSession, FetchOptions::FetchSources({source})},
      },
      base::Time::Now() - base::Days(1));
  auto result = FetchAndGetResult(options);
  EXPECT_EQ(result.status, FetchResult::Status::kSuccess);
  EXPECT_EQ(result.data.size(), 2u);
  for (const auto& url_key : {kSampleSearchUrl, kSampleSearchUrl2}) {
    const auto& tab_data =
        std::get<URLVisitAggregate::TabData>(result.data.at(url_key));
    EXPECT_EQ(tab_data.tab_count, 1u);
    EXPECT_EQ(tab_data.last_active_tab.visit.source, source);
  }
  EXPECT_EQ(
      std::get<URLVisitAggregate::TabData>(result.data.at(kSampleSearchUrl))
          .pinned,
      true);
  EXPECT_EQ(
      std::get<URLVisitAggregate::TabData>(result.data.at(kSampleSearchUrl2))
          .pinned,
      false);
}

}  // namespace visited_url_ranking
