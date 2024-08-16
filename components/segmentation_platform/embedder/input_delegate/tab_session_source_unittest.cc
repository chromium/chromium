// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include <math.h>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace segmentation_platform::processing {
namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kLocalTabName[] = "local";
constexpr char kRemoteTabName1[] = "remote_1";
constexpr char kRemoteTabName2[] = "remote_2";
const base::TimeDelta kTime1 = base::Minutes(15);
const base::TimeDelta kTime2 = base::Minutes(10);
const base::TimeDelta kTime3 = base::Minutes(5);

std::unique_ptr<sync_sessions::SyncedSession> CreateNewSession(
    const std::string& tab_guid,
    const base::Time& session_time) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  session->SetModifiedTime(session_time);
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();
  tab->timestamp = session_time;
  tab->guid = tab_guid;
  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
  return session;
}

class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MockSessionSyncService() = default;
  ~MockSessionSyncService() override = default;

  MOCK_METHOD(syncer::GlobalIdMapper*,
              GetGlobalIdMapper,
              (),
              (const, override));
  MOCK_METHOD(sync_sessions::OpenTabsUIDelegate*,
              GetOpenTabsUIDelegate,
              (),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              SubscribeToForeignSessionsChanged,
              (const base::RepeatingClosure& cb),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate() {
    local_session_ =
        CreateNewSession(kLocalTabName, base::Time::Now() - kTime2);
    foreign_sessions_owned_.push_back(
        CreateNewSession(kRemoteTabName1, base::Time::Now() - kTime1));
    foreign_sessions_.push_back(foreign_sessions_owned_.back().get());
    foreign_sessions_owned_.push_back(
        CreateNewSession(kRemoteTabName2, base::Time::Now() - kTime3));
    foreign_sessions_.push_back(foreign_sessions_owned_.back().get());
  }

  bool GetAllForeignSessions(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>>* sessions) override {
    *sessions = foreign_sessions_;
    base::ranges::sort(*sessions, std::greater(),
                       [](const sync_sessions::SyncedSession* session) {
                         return session->GetModifiedTime();
                       });

    return !sessions->empty();
  }

  bool GetLocalSession(
      const sync_sessions::SyncedSession** local_session) override {
    *local_session = local_session_.get();
    return *local_session != nullptr;
  }

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

 private:
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>>
      foreign_sessions_owned_;
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions_;
  std::unique_ptr<sync_sessions::SyncedSession> local_session_;
};

}  // namespace

class TabSessionSourceTest : public testing::Test {
 public:
  TabSessionSourceTest() = default;
  ~TabSessionSourceTest() override = default;

  void SetUp() override {
    Test::SetUp();
    tab_fetcher_ = std::make_unique<TabFetcher>(&session_sync_service_);
    tab_source_ = std::make_unique<TabSessionSource>(&session_sync_service_,
                                                     tab_fetcher_.get());
    EXPECT_CALL(session_sync_service_, GetOpenTabsUIDelegate())
        .WillRepeatedly(Return(&open_tabs_delegate_));
  }

  void TearDown() override {
    Test::TearDown();
    tab_source_.reset();
  }

  Tensor GetResult(const std::string& session_tag, const SessionID& tab_id) {
    scoped_refptr<InputContext> input_context =
        base::MakeRefCounted<InputContext>();
    input_context->metadata_args.emplace(
        "session_tag", processing::ProcessedValue(session_tag));
    input_context->metadata_args.emplace(
        "tab_id", processing::ProcessedValue(tab_id.id()));
    proto::SegmentationModelMetadata metadata;
    MetadataWriter writer(&metadata);
    proto::CustomInput* input =
        writer.AddCustomInput(MetadataWriter::CustomInput{
            .tensor_length = 7,
            .fill_policy = proto::CustomInput::FILL_TAB_METRICS,
            .name = "tab"});
    FeatureProcessorState state;
    state.set_input_context_for_testing(input_context);

    base::RunLoop wait;
    Tensor result;
    tab_source_->Process(*input, state,
                         base::BindOnce(
                             [](Tensor* result, base::OnceClosure quit_closure,
                                bool success, Tensor features) {
                               result->swap(features);
                               std::move(quit_closure).Run();
                             },
                             &result, wait.QuitClosure()));
    wait.Run();
    return result;
  }

 protected:
  base::test::TaskEnvironment task_env_;
  MockSessionSyncService session_sync_service_;
  std::unique_ptr<TabFetcher> tab_fetcher_;
  MockOpenTabsUIDelegate open_tabs_delegate_;
  std::unique_ptr<TabSessionSource> tab_source_;
};

TEST_F(TabSessionSourceTest, Bucketize) {
  EXPECT_NEAR(TabSessionSource::BucketizeExp(0, /*max_buckets*/50), 0, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeLinear(0, /*max_buckets*/10), 0, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(1, /*max_buckets*/50), 1, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeLinear(1, /*max_buckets*/10), 1, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(5, /*max_buckets*/50), 4, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeLinear(5, /*max_buckets*/10), 5, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(10, /*max_buckets*/50), 8, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeLinear(10, /*max_buckets*/10), 10, 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(pow(2, 60), /*max_buckets*/50), pow(2, 50), 0.01);
  EXPECT_NEAR(TabSessionSource::BucketizeLinear(16, /*max_buckets*/10), 10, 0.01);
}

TEST_F(TabSessionSourceTest, ProcessLocal) {
  const sync_sessions::SyncedSession* local_session = nullptr;
  ASSERT_TRUE(session_sync_service_.GetOpenTabsUIDelegate()->GetLocalSession(
      &local_session));
  auto& picked_tab =
      local_session->windows.begin()->second->wrapped_window.tabs[0];
  const base::TimeDelta kNavTime1 = base::Seconds(40);
  const base::TimeDelta kNavTime2 = base::Seconds(25);
  picked_tab->navigations.emplace_back();
  picked_tab->navigations.back().set_timestamp(base::Time::Now() - kNavTime1);
  picked_tab->navigations.emplace_back();
  picked_tab->navigations.back().set_timestamp(base::Time::Now() - kNavTime2);
  picked_tab->navigations.back().set_transition_type(
      ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  picked_tab->current_navigation_index = 1;
  SessionID id = picked_tab->tab_id;

  EXPECT_CALL(open_tabs_delegate_, GetForeignTab(_, _, _))
      .WillOnce([&picked_tab](const std::string& tag, SessionID tab_id,
                              const sessions::SessionTab** tab) {
        *tab = picked_tab.get();
        return true;
      });
  EXPECT_CALL(open_tabs_delegate_, GetForeignSession(_))
      .WillOnce([&local_session](const std::string& tag) {
        std::vector<const sessions::SessionWindow*> windows;
        for (const auto& [window_id, window] : local_session->windows) {
          windows.push_back(&window->wrapped_window);
        }
        return windows;
      });

  Tensor result = GetResult(local_session->GetSessionTag(), id);
  EXPECT_NEAR(result[TabSessionSource::kInputTimeSinceModifiedSec].float_val,
              TabSessionSource::BucketizeExp(kTime2.InSecondsF(), 50), 0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputTimeSinceLastNavSec].float_val,
              TabSessionSource::BucketizeExp(kNavTime2.InSecondsF(), 50),
              0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputTimeSinceFirstNavSec].float_val,
              TabSessionSource::BucketizeExp(kNavTime1.InSecondsF(), 50),
              0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputLastTransitionType].float_val,
              static_cast<int>(ui::PAGE_TRANSITION_AUTO_SUBFRAME), 0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputTabRankInSession].float_val, 0,
              0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputSessionRank].float_val, 1, 0.001);
}

TEST_F(TabSessionSourceTest, ProcessForeign) {
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions;
  ASSERT_TRUE(
      session_sync_service_.GetOpenTabsUIDelegate()->GetAllForeignSessions(
          &foreign_sessions));
  const auto* picked_session = foreign_sessions[0].get();
  auto& picked_tab =
      picked_session->windows.begin()->second->wrapped_window.tabs[0];
  const base::TimeDelta kNavTime1 = base::Seconds(40);
  picked_tab->navigations.emplace_back();
  picked_tab->navigations.back().set_timestamp(base::Time::Now() - kNavTime1);
  picked_tab->current_navigation_index = 1;
  SessionID id = picked_tab->tab_id;

  EXPECT_CALL(open_tabs_delegate_, GetForeignTab(_, _, _))
      .WillOnce([&picked_tab](const std::string& tag, SessionID tab_id,
                              const sessions::SessionTab** tab) {
        *tab = picked_tab.get();
        return true;
      });
  EXPECT_CALL(open_tabs_delegate_, GetForeignSession(_))
      .WillOnce([&picked_session](const std::string& tag) {
        std::vector<const sessions::SessionWindow*> windows;
        for (const auto& [window_id, window] : picked_session->windows) {
          windows.push_back(&window->wrapped_window);
        }
        return windows;
      });

  Tensor result = GetResult(picked_session->GetSessionTag(), id);
  EXPECT_NEAR(result[TabSessionSource::kInputTimeSinceModifiedSec].float_val,
              TabSessionSource::BucketizeExp(kTime3.InSecondsF(), 50), 0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputTimeSinceLastNavSec].float_val,
              TabSessionSource::BucketizeExp(kNavTime1.InSecondsF(), 50),
              0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputTimeSinceFirstNavSec].float_val,
              TabSessionSource::BucketizeExp(kNavTime1.InSecondsF(), 50),
              0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputLastTransitionType].float_val,
              static_cast<int>(ui::PAGE_TRANSITION_TYPED), 0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputTabRankInSession].float_val, 0,
              0.001);
  EXPECT_NEAR(result[TabSessionSource::kInputSessionRank].float_val, 0, 0.001);
}

}  // namespace segmentation_platform::processing
