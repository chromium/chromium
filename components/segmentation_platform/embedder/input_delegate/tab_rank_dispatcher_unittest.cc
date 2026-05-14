// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/mock_session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using ::testing::_;
using ::testing::Return;
using MockSessionSyncService = sync_sessions::MockSessionSyncService;

constexpr char kLocalTabName[] = "local";
constexpr char kRemoteTabName1[] = "remote_1";
constexpr char kRemoteTabName2[] = "remote_2";
const base::TimeDelta kTime1 = base::Minutes(15);
const base::TimeDelta kTime2 = base::Minutes(10);
const base::TimeDelta kTime3 = base::Minutes(5);
const TrainingRequestId kTestRequestId = TrainingRequestId::FromUnsafeValue(5);

void AddTabToSession(sync_sessions::SyncedSession* session,
                     const std::string& tab_guid,
                     const base::Time& session_time) {
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();
  tab->timestamp = session_time;
  tab->guid = tab_guid;
  tab->tab_id = SessionID::NewUnique();
  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
}

AnnotatedNumericResult CreateResult(float val) {
  AnnotatedNumericResult result(PredictionStatus::kSucceeded);
  result.result.mutable_output_config()
      ->mutable_predictor()
      ->mutable_generic_predictor()
      ->add_output_labels(kTabResumptionClassifierKey);
  result.result.add_result(val);
  result.request_id = kTestRequestId;
  return result;
}

}  // namespace

class TabRankDispatcherTest : public testing::Test {
 public:
  TabRankDispatcherTest() {
    EXPECT_CALL(session_sync_service_, GetOpenTabsUIDelegate())
        .WillRepeatedly(Return(&open_tabs_delegate_));

    const base::Time now = base::Time::Now();
    open_tabs_delegate_.local_session()->SetSessionTag(kLocalTabName);
    open_tabs_delegate_.local_session()->SetModifiedTime(now - kTime2);
    AddTabToSession(open_tabs_delegate_.local_session(), kLocalTabName,
                    now - kTime2);

    AddTabToSession(
        open_tabs_delegate_.AddForeignSession(kRemoteTabName1, now - kTime1),
        kRemoteTabName1, now - kTime1);

    AddTabToSession(
        open_tabs_delegate_.AddForeignSession(kRemoteTabName2, now - kTime3),
        kRemoteTabName2, now - kTime3);
  }
  ~TabRankDispatcherTest() override = default;

 protected:
  MockSessionSyncService session_sync_service_;
  sync_sessions::FakeOpenTabsUIDelegate open_tabs_delegate_;
  MockSegmentationPlatformService segmentation_service_;
};

TEST_F(TabRankDispatcherTest, RankTabs) {
  TabRankDispatcher dispatcher(
      &segmentation_service_, &session_sync_service_,
      std::make_unique<TabFetcher>(&session_sync_service_));
  TabRankDispatcher::TabFilter filter;

  testing::InSequence s;
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.3f)));
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.5f)));
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.6f)));

  dispatcher.GetTopRankedTabs(
      kTabResumptionClassifierKey, filter,
      base::BindOnce(
          [](TabFetcher* fetcher, bool success,
             std::multiset<TabRankDispatcher::RankedTab> tabs) {
            ASSERT_EQ(tabs.size(), 3u);
            auto tab1 = fetcher->FindTab(tabs.begin()->tab);
            auto tab2 = fetcher->FindTab((++tabs.begin())->tab);
            auto tab3 = fetcher->FindTab(tabs.rbegin()->tab);
            EXPECT_EQ(tab1.session_tab->guid, kLocalTabName);
            EXPECT_NEAR(tabs.begin()->model_score, 0.6, 0.001);
            EXPECT_EQ(tab2.session_tab->guid, kRemoteTabName1);
            EXPECT_NEAR((++tabs.begin())->model_score, 0.5, 0.001);
            EXPECT_EQ(tab3.session_tab->guid, kRemoteTabName2);
            EXPECT_NEAR(tabs.rbegin()->model_score, 0.3, 0.001);
            EXPECT_EQ(tabs.begin()->request_id, kTestRequestId);
          },
          dispatcher.tab_fetcher()));
}

TEST_F(TabRankDispatcherTest, FilterTabs) {
  TabRankDispatcher dispatcher(
      &segmentation_service_, &session_sync_service_,
      std::make_unique<TabFetcher>(&session_sync_service_));
  TabRankDispatcher::TabFilter filter =
      base::BindRepeating([](const TabFetcher::Tab& tab) {
        return tab.time_since_modified <= kTime2 + base::Seconds(1);
      });

  testing::InSequence s;
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.5f)));
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.6f)));

  dispatcher.GetTopRankedTabs(
      kTabResumptionClassifierKey, filter,
      base::BindOnce(
          [](TabFetcher* fetcher, bool success,
             std::multiset<TabRankDispatcher::RankedTab> tabs) {
            ASSERT_EQ(tabs.size(), 2u);
            auto tab1 = fetcher->FindTab(tabs.begin()->tab);
            auto tab2 = fetcher->FindTab(tabs.rbegin()->tab);
            EXPECT_EQ(tab1.session_tab->guid, kLocalTabName);
            EXPECT_NEAR(tabs.begin()->model_score, 0.6, 0.001);
            EXPECT_EQ(tab2.session_tab->guid, kRemoteTabName2);
            EXPECT_NEAR(tabs.rbegin()->model_score, 0.5, 0.001);
          },
          dispatcher.tab_fetcher()));
}

TEST_F(TabRankDispatcherTest, TabsWithSameScore) {
  TabRankDispatcher dispatcher(
      &segmentation_service_, &session_sync_service_,
      std::make_unique<TabFetcher>(&session_sync_service_));
  TabRankDispatcher::TabFilter filter;

  testing::InSequence s;
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.3f)));
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.3f)));
  EXPECT_CALL(segmentation_service_, GetAnnotatedNumericResult(_, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(CreateResult(0.6f)));

  dispatcher.GetTopRankedTabs(
      kTabResumptionClassifierKey, filter,
      base::BindOnce(
          [](TabFetcher* fetcher, bool success,
             std::multiset<TabRankDispatcher::RankedTab> tabs) {
            ASSERT_EQ(tabs.size(), 3u);
            auto tab1 = fetcher->FindTab(tabs.begin()->tab);
            auto tab2 = fetcher->FindTab((++tabs.begin())->tab);
            auto tab3 = fetcher->FindTab(tabs.rbegin()->tab);
            EXPECT_EQ(tab1.session_tab->guid, kLocalTabName);
            EXPECT_NEAR(tabs.begin()->model_score, 0.6, 0.001);
            EXPECT_TRUE(base::StartsWith(tab2.session_tab->guid, "remote_"));
            EXPECT_NEAR((++tabs.begin())->model_score, 0.3, 0.001);
            EXPECT_TRUE(base::StartsWith(tab3.session_tab->guid, "remote_"));
            EXPECT_NEAR(tabs.rbegin()->model_score, 0.3, 0.001);
          },
          dispatcher.tab_fetcher()));
}

}  // namespace segmentation_platform
