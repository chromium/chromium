// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_service.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace critical_actions {

class CriticalActionServiceTest : public testing::Test {
 public:
  CriticalActionServiceTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    service_ =
        std::make_unique<CriticalActionService>(db_path_, backend_task_runner_);
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
    task_environment_.RunUntilIdle();
  }

  CriticalActionEntry CreateDefaultEntry(
      ActionType action_type = ActionType::kFormFill,
      ActionSource source = ActionSource::kActor) {
    CriticalActionEntry entry;
    entry.critical_action_id =
        base::Uuid::GenerateRandomV4().AsLowercaseString();
    entry.timestamp = base::Time::Now();
    entry.action_type = action_type;
    entry.action_source = source;
    return entry;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<CriticalActionService> service_;
};

// Verifies end-to-end integration and that callbacks run on the main thread.
TEST_F(CriticalActionServiceTest, AddAndGetActionRunsOnMainThread) {
  CriticalActionEntry entry =
      CreateDefaultEntry(ActionType::kCredentialAccess, ActionSource::kUnknown);
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.actor_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.metadata = "{\"scopes\": [\"profile\"]}";

  // AddCriticalAction does not have a callback (it is asynchronous
  // fire-and-forget). Because backend operations run on a sequenced task
  // runner, the subsequent GetCriticalAction is guaranteed to run after
  // AddCriticalAction completes.
  service_->AddCriticalAction(entry);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  scoped_refptr<base::SequencedTaskRunner> original_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  service_->GetCriticalAction(
      entry.critical_action_id,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> original_runner,
             base::OnceCallback<void(std::optional<CriticalActionEntry>)>
                 callback,
             std::optional<CriticalActionEntry> retrieved) {
            EXPECT_TRUE(original_runner->RunsTasksInCurrentSequence());
            std::move(callback).Run(retrieved);
          },
          original_runner, get_future.GetCallback()));
  auto retrieved = get_future.Get();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);
}

// Verifies that service APIs behave gracefully after Service Shutdown.
TEST_F(CriticalActionServiceTest, CallsAfterShutdownGracefullyFail) {
  service_->Shutdown();

  const std::string action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry = CreateDefaultEntry();
  entry.critical_action_id = action_id;

  // The following calls should be safe no-ops and not crash.
  service_->AddCriticalAction(entry);
  service_->DeleteCriticalAction(action_id);
  service_->DeleteCriticalActionsInTimeRange(base::Time(), base::Time());
  service_->DeleteCriticalActionsByVisitIds({123, 456});

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  service_->GetCriticalAction(action_id, get_future.GetCallback());
  EXPECT_FALSE(get_future.Get().has_value());

  task_environment_.RunUntilIdle();
}

TEST_F(CriticalActionServiceTest, DeleteAllHistoryDeletesEverything) {
  CriticalActionEntry entry = CreateDefaultEntry(ActionType::kCredentialAccess);
  service_->AddCriticalAction(entry);

  service_->OnHistoryDeletions(nullptr, history::DeletionInfo::ForAllHistory());

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  service_->GetCriticalAction(entry.critical_action_id,
                              get_future.GetCallback());
  EXPECT_FALSE(get_future.Get().has_value());
}

TEST_F(CriticalActionServiceTest, DeleteHistoryByRange) {
  base::Time start_time = base::Time::Now();
  base::Time action_time = start_time + base::Minutes(5);
  base::Time end_time = start_time + base::Minutes(10);

  CriticalActionEntry entry1 =
      CreateDefaultEntry(ActionType::kCredentialAccess);
  entry1.timestamp = action_time;
  service_->AddCriticalAction(entry1);

  CriticalActionEntry entry2 =
      CreateDefaultEntry(ActionType::kCredentialAccess);
  entry2.timestamp = end_time + base::Minutes(5);
  service_->AddCriticalAction(entry2);

  history::DeletionTimeRange time_range(start_time, end_time);
  history::DeletionInfo deletion_info(time_range, /*is_from_expiration=*/false,
                                      history::DeletionInfo::Reason::kOther,
                                      /*deleted_rows=*/{},
                                      /*deleted_visit_ids=*/{},
                                      /*favicon_urls=*/{},
                                      /*restrict_urls=*/std::nullopt);
  service_->OnHistoryDeletions(nullptr, deletion_info);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future1;
  service_->GetCriticalAction(entry1.critical_action_id,
                              get_future1.GetCallback());
  EXPECT_FALSE(get_future1.Get().has_value());

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future2;
  service_->GetCriticalAction(entry2.critical_action_id,
                              get_future2.GetCallback());
  EXPECT_TRUE(get_future2.Get().has_value());
}

TEST_F(CriticalActionServiceTest, DeleteHistoryByVisitId) {
  int64_t visit_id_to_delete = base::RandIntInclusive(1, 1000000);
  int64_t visit_id_to_keep = visit_id_to_delete + 1;

  CriticalActionEntry entry1 =
      CreateDefaultEntry(ActionType::kCredentialAccess);
  entry1.visit_id = visit_id_to_delete;
  service_->AddCriticalAction(entry1);

  CriticalActionEntry entry2 =
      CreateDefaultEntry(ActionType::kCredentialAccess);
  entry2.visit_id = visit_id_to_keep;
  service_->AddCriticalAction(entry2);

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange::Invalid(),
      /*is_from_expiration=*/false, history::DeletionInfo::Reason::kOther,
      /*deleted_rows=*/{},
      /*deleted_visit_ids=*/{visit_id_to_delete},
      /*favicon_urls=*/{},
      /*restrict_urls=*/std::nullopt);
  service_->OnHistoryDeletions(nullptr, deletion_info);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future1;
  service_->GetCriticalAction(entry1.critical_action_id,
                              get_future1.GetCallback());
  EXPECT_FALSE(get_future1.Get().has_value());

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future2;
  service_->GetCriticalAction(entry2.critical_action_id,
                              get_future2.GetCallback());
  EXPECT_TRUE(get_future2.Get().has_value());
}

TEST_F(CriticalActionServiceTest, GetCriticalActionsWithOptions) {
  CriticalActionEntry entry1 = CreateDefaultEntry();
  entry1.visit_id = 1;
  service_->AddCriticalAction(entry1);

  CriticalActionEntry entry2 = CreateDefaultEntry(ActionType::kDownload);
  entry2.visit_id = 2;
  service_->AddCriticalAction(entry2);

  // Get both entries.
  base::test::TestFuture<std::vector<CriticalActionEntry>> get_future1;
  CriticalActionQueryOptions options;
  service_->GetCriticalActions(options, get_future1.GetCallback());
  auto results1 = get_future1.Get();
  ASSERT_EQ(results1.size(), 2u);

  // Filter by action_type kDownload.
  base::test::TestFuture<std::vector<CriticalActionEntry>> get_future2;
  CriticalActionQueryOptions options2;
  options2.action_types = {ActionType::kDownload};
  service_->GetCriticalActions(options2, get_future2.GetCallback());
  auto results2 = get_future2.Get();
  ASSERT_EQ(results2.size(), 1u);
  EXPECT_EQ(results2[0].critical_action_id, entry2.critical_action_id);
}

TEST_F(CriticalActionServiceTest, AddCriticalActionWithNavigationIdInOrder) {
  int64_t nav_id = 1001;
  int64_t visit_id = 54321;
  CriticalActionEntry entry = CreateDefaultEntry();

  // Action added before visit_id resolution.
  service_->AddCriticalActionWithNavigationId(entry, nav_id);

  // History callback arrives.
  history::URLRow url_row(GURL("https://example.com/login"));
  history::VisitRow visit_row;
  visit_row.visit_id = visit_id;
  history::VisitedURLInfo visited_info(
      url_row, visit_row, history::VisitResponseCodeCategory::kNot404, nav_id);
  service_->OnURLVisitedWithNavigationId(nullptr, visited_info);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  service_->GetCriticalAction(entry.critical_action_id,
                              get_future.GetCallback());
  auto retrieved = get_future.Get();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->visit_id, visit_id);
}

TEST_F(CriticalActionServiceTest,
       AddCriticalActionWithNavigationIdPreResolved) {
  int64_t nav_id = 1002;
  int64_t visit_id = 98765;

  // History callback arrives first.
  history::URLRow url_row(GURL("https://example.com/register"));
  history::VisitRow visit_row;
  visit_row.visit_id = visit_id;
  history::VisitedURLInfo visited_info(
      url_row, visit_row, history::VisitResponseCodeCategory::kNot404, nav_id);
  service_->OnURLVisitedWithNavigationId(nullptr, visited_info);

  // Action added after visit_id resolution.
  CriticalActionEntry entry = CreateDefaultEntry();
  service_->AddCriticalActionWithNavigationId(entry, nav_id);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  service_->GetCriticalAction(entry.critical_action_id,
                              get_future.GetCallback());
  auto retrieved = get_future.Get();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->visit_id, visit_id);
}

struct VisitIdResolutionTestCase {
  std::string test_name;
  VisitIdResolutionOutcome expected_outcome;
  void (*trigger)(CriticalActionService* service,
                  const CriticalActionEntry& entry,
                  int64_t nav_id);
};

class VisitIdResolutionOutcomeTest
    : public CriticalActionServiceTest,
      public testing::WithParamInterface<VisitIdResolutionTestCase> {};

TEST_P(VisitIdResolutionOutcomeTest, EmitsExpectedOutcome) {
  const VisitIdResolutionTestCase& test_case = GetParam();
  base::HistogramTester histogram_tester;
  int64_t nav_id = 1000;
  CriticalActionEntry entry =
      CreateDefaultEntry(ActionType::kFormFill, ActionSource::kPasswordManager);

  test_case.trigger(service_.get(), entry, nav_id);

  histogram_tester.ExpectUniqueSample(
      "CriticalActions.VisitIdResolutionOutcome.PasswordManager",
      test_case.expected_outcome, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VisitIdResolutionOutcomeTest,
    testing::Values(
        VisitIdResolutionTestCase{
            .test_name = "Success",
            .expected_outcome = VisitIdResolutionOutcome::kSuccess,
            .trigger = [](CriticalActionService* service,
                          const CriticalActionEntry& entry, int64_t nav_id) {
              int64_t visit_id = 12345;
              service->AddCriticalActionWithNavigationId(entry, nav_id);
              history::URLRow url_row(GURL("https://example.com/login"));
              history::VisitRow visit_row;
              visit_row.visit_id = visit_id;
              history::VisitedURLInfo visited_info(
                  url_row, visit_row,
                  history::VisitResponseCodeCategory::kNot404, nav_id);
              service->OnURLVisitedWithNavigationId(nullptr, visited_info);
            }},
        VisitIdResolutionTestCase{
            .test_name = "EvictedNavigatedAway",
            .expected_outcome = VisitIdResolutionOutcome::kEvictedNavigatedAway,
            .trigger = [](CriticalActionService* service,
                          const CriticalActionEntry& entry, int64_t nav_id) {
              service->AddCriticalActionWithNavigationId(entry, nav_id);
              service->OnNavigationDiscarded(nav_id);
            }},
        VisitIdResolutionTestCase{
            .test_name = "DroppedNoNavigationId",
            .expected_outcome =
                VisitIdResolutionOutcome::kDroppedNoNavigationId,
            .trigger = [](CriticalActionService* service,
                          const CriticalActionEntry& entry, int64_t nav_id) {
              service->AddCriticalActionWithNavigationId(entry,
                                                         /*navigation_id=*/0);
            }},
        VisitIdResolutionTestCase{
            .test_name = "EvictedServiceShutdown",
            .expected_outcome =
                VisitIdResolutionOutcome::kEvictedServiceShutdown,
            .trigger = [](CriticalActionService* service,
                          const CriticalActionEntry& entry, int64_t nav_id) {
              service->AddCriticalActionWithNavigationId(entry, nav_id);
              service->Shutdown();
            }}),
    [](const testing::TestParamInfo<VisitIdResolutionTestCase>& info) {
      return info.param.test_name;
    });

TEST_F(CriticalActionServiceTest,
       VisitIdResolutionOutcomeEvictedCapacityExceeded) {
  base::HistogramTester histogram_tester;
  int64_t nav_id = 1000;
  CriticalActionEntry lru_entry =
      CreateDefaultEntry(ActionType::kFormFill, ActionSource::kPasswordManager);
  service_->AddCriticalActionWithNavigationId(lru_entry, nav_id);

  CriticalActionEntry other_entry =
      CreateDefaultEntry(ActionType::kFormFill, ActionSource::kAutofill);

  const int max_capacity = features::kMaxNavigationCacheCapacity.Get();
  for (int64_t next_id = nav_id + 1; next_id <= nav_id + max_capacity;
       ++next_id) {
    service_->AddCriticalActionWithNavigationId(other_entry, next_id);
  }

  histogram_tester.ExpectBucketCount(
      "CriticalActions.VisitIdResolutionOutcome.PasswordManager",
      VisitIdResolutionOutcome::kEvictedCapacityExceeded, 1);
  histogram_tester.ExpectBucketCount(
      "CriticalActions.VisitIdResolutionOutcome.Autofill",
      VisitIdResolutionOutcome::kEvictedCapacityExceeded, 0);
}

TEST_F(CriticalActionServiceTest,
       VisitIdResolutionOutcomeEvictedCapacityExceededLruOrder) {
  base::HistogramTester histogram_tester;
  int64_t nav_id_1 = 1000;
  int64_t nav_id_2 = 1001;
  int64_t lru_nav_id = 1002;

  CriticalActionEntry other_entry =
      CreateDefaultEntry(ActionType::kFormFill, ActionSource::kAutofill);
  CriticalActionEntry lru_entry =
      CreateDefaultEntry(ActionType::kFormFill, ActionSource::kPasswordManager);

  // Add initial entries for nav_id_1 and nav_id_2.
  service_->AddCriticalActionWithNavigationId(other_entry, nav_id_1);
  service_->AddCriticalActionWithNavigationId(other_entry, nav_id_2);

  // Add the entry that should eventually become the least recently used.
  service_->AddCriticalActionWithNavigationId(lru_entry, lru_nav_id);

  // Re-access nav_id_1 and nav_id_2 so they become more recently used than
  // lru_entry, testing true LRU behavior (recency) over insertion order.
  service_->AddCriticalActionWithNavigationId(other_entry, nav_id_1);
  service_->AddCriticalActionWithNavigationId(other_entry, nav_id_2);

  // Add more entries until capacity is exceeded by 1.
  const int max_capacity = features::kMaxNavigationCacheCapacity.Get();
  for (int64_t next_id = lru_nav_id + 1;
       next_id < lru_nav_id + 1 + (max_capacity - 2); ++next_id) {
    service_->AddCriticalActionWithNavigationId(other_entry, next_id);
  }

  histogram_tester.ExpectBucketCount(
      "CriticalActions.VisitIdResolutionOutcome.PasswordManager",
      VisitIdResolutionOutcome::kEvictedCapacityExceeded, 1);
  histogram_tester.ExpectBucketCount(
      "CriticalActions.VisitIdResolutionOutcome.Autofill",
      VisitIdResolutionOutcome::kEvictedCapacityExceeded, 0);
}

TEST_F(CriticalActionServiceTest, EventLoggedHistogramEmitted) {
  base::HistogramTester histogram_tester;

  CriticalActionEntry actor_entry = CreateDefaultEntry();
  actor_entry.visit_id = 1001;
  actor_entry.action_type = ActionType::kGooglePasswordManager;
  service_->AddCriticalAction(actor_entry);

  CriticalActionEntry pwm_entry =
      CreateDefaultEntry(ActionType::kFormFill, ActionSource::kPasswordManager);
  pwm_entry.visit_id = 1002;
  service_->AddCriticalAction(pwm_entry);

  CriticalActionEntry autofill_entry =
      CreateDefaultEntry(ActionType::kFormFill, ActionSource::kAutofill);
  autofill_entry.visit_id = 1003;
  service_->AddCriticalAction(autofill_entry);

  histogram_tester.ExpectUniqueSample("CriticalActions.EventLogged.Actor",
                                      ActionType::kGooglePasswordManager, 1);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.EventLogged.PasswordManager", ActionType::kFormFill, 1);
  histogram_tester.ExpectUniqueSample("CriticalActions.EventLogged.Autofill",
                                      ActionType::kFormFill, 1);
}

struct CriticalActionServiceConversationIdTestCase {
  std::string test_name;
  std::optional<int64_t> expected_visit_id;
  void (*execute_flow)(CriticalActionService* service,
                       const CriticalActionEntry& entry,
                       const std::string& task_id,
                       const std::string& conv_id);
};

class CriticalActionServiceConversationIdTest
    : public CriticalActionServiceTest,
      public testing::WithParamInterface<
          CriticalActionServiceConversationIdTestCase> {};

TEST_P(CriticalActionServiceConversationIdTest,
       AssociatesConversationIdCorrectly) {
  const CriticalActionServiceConversationIdTestCase& test_case = GetParam();
  const std::string task_id = "test_task";
  const std::string conv_id = "test_conv";
  CriticalActionEntry entry = CreateDefaultEntry();
  entry.actor_task_id = task_id;

  test_case.execute_flow(service_.get(), entry, task_id, conv_id);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  service_->GetCriticalAction(entry.critical_action_id,
                              get_future.GetCallback());
  auto retrieved = get_future.Get();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->conversation_id, conv_id);
  if (test_case.expected_visit_id.has_value()) {
    EXPECT_EQ(retrieved->visit_id, test_case.expected_visit_id);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CriticalActionServiceConversationIdTest,
    testing::Values(
        CriticalActionServiceConversationIdTestCase{
            .test_name = "BeforeAction",
            .execute_flow =
                [](CriticalActionService* service,
                   const CriticalActionEntry& entry,
                   const std::string& task_id,
                   const std::string& conv_id) {
                  service->SetCriticalActionsConversationId({task_id}, conv_id);
                  service->AddCriticalAction(entry);
                }},
        CriticalActionServiceConversationIdTestCase{
            .test_name = "BeforeNavigationResolution",
            .expected_visit_id = 999,
            .execute_flow =
                [](CriticalActionService* service,
                   const CriticalActionEntry& entry,
                   const std::string& task_id,
                   const std::string& conv_id) {
                  int64_t nav_id = 555;
                  int64_t visit_id = 999;
                  service->AddCriticalActionWithNavigationId(entry, nav_id);
                  service->SetCriticalActionsConversationId({task_id}, conv_id);
                  history::URLRow url_row(GURL("https://example.com/step"));
                  history::VisitRow visit_row;
                  visit_row.visit_id = visit_id;
                  history::VisitedURLInfo visited_info(
                      url_row, visit_row,
                      history::VisitResponseCodeCategory::kNot404, nav_id);
                  service->OnURLVisitedWithNavigationId(nullptr, visited_info);
                }},
        CriticalActionServiceConversationIdTestCase{
            .test_name = "AfterAction",
            .execute_flow =
                [](CriticalActionService* service,
                   const CriticalActionEntry& entry,
                   const std::string& task_id,
                   const std::string& conv_id) {
                  service->AddCriticalAction(entry);
                  service->SetCriticalActionsConversationId({task_id}, conv_id);
                }}),
    [](const testing::TestParamInfo<
        CriticalActionServiceConversationIdTestCase>& info) {
      return info.param.test_name;
    });

TEST_F(CriticalActionServiceTest,
       SubsequentActionAfterConversationRegistered) {
  const std::string task_id = "task_multi_action";
  const std::string conv_id = "conv_multi_action";

  // First action occurs before conversation is registered.
  CriticalActionEntry entry1 = CreateDefaultEntry();
  entry1.actor_task_id = task_id;
  service_->AddCriticalAction(entry1);

  // Conversation is registered.
  service_->SetCriticalActionsConversationId({task_id}, conv_id);

  // Second action occurs after conversation is registered.
  CriticalActionEntry entry2 = CreateDefaultEntry();
  entry2.actor_task_id = task_id;
  service_->AddCriticalAction(entry2);

  // Verify both entries received the conversation_id.
  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future1;
  service_->GetCriticalAction(entry1.critical_action_id,
                              get_future1.GetCallback());
  auto retrieved1 = get_future1.Get();
  ASSERT_TRUE(retrieved1.has_value());
  EXPECT_EQ(retrieved1->conversation_id, conv_id);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future2;
  service_->GetCriticalAction(entry2.critical_action_id,
                              get_future2.GetCallback());
  auto retrieved2 = get_future2.Get();
  ASSERT_TRUE(retrieved2.has_value());
  EXPECT_EQ(retrieved2->conversation_id, conv_id);
}

namespace {

struct SetCriticalActionsConversationIdServiceTestCase {
  std::string test_name;
  std::vector<std::string> actor_task_ids_to_resolve;
  std::vector<std::string> entry_task_ids;
  base::flat_map<std::string, std::string> expected_conversation_ids;
};

}  // namespace

class SetCriticalActionsConversationIdServiceTest
    : public CriticalActionServiceTest,
      public ::testing::WithParamInterface<
          SetCriticalActionsConversationIdServiceTestCase> {};

TEST_P(SetCriticalActionsConversationIdServiceTest,
       SetCriticalActionsConversationId) {
  const SetCriticalActionsConversationIdServiceTestCase& test_case = GetParam();
  const std::string kConvId = "conv_resolved1";

  std::vector<CriticalActionEntry> entries;
  for (const auto& task_id : test_case.entry_task_ids) {
    CriticalActionEntry entry = CreateDefaultEntry();
    entry.actor_task_id = task_id;
    service_->AddCriticalAction(entry);
    entries.push_back(entry);
  }

  // Before resolution, none of the entries should have a conversation ID.
  for (const auto& entry : entries) {
    SCOPED_TRACE(entry.actor_task_id);
    base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
    service_->GetCriticalAction(entry.critical_action_id,
                                get_future.GetCallback());
    auto retrieved = get_future.Get();
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->conversation_id.empty());
  }

  service_->SetCriticalActionsConversationId(
      test_case.actor_task_ids_to_resolve, kConvId);

  for (const auto& entry : entries) {
    SCOPED_TRACE(entry.actor_task_id);
    base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
    service_->GetCriticalAction(entry.critical_action_id,
                                get_future.GetCallback());
    auto retrieved = get_future.Get();
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->conversation_id,
              test_case.expected_conversation_ids.at(entry.actor_task_id));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SetCriticalActionsConversationIdServiceTest,
    testing::Values(
        SetCriticalActionsConversationIdServiceTestCase{
            .test_name = "SubsetResolves",
            .actor_task_ids_to_resolve = {"task_1", "task_2"},
            .entry_task_ids = {"task_1", "task_2", "task_other"},
            .expected_conversation_ids = {{"task_1", "conv_resolved1"},
                                          {"task_2", "conv_resolved1"},
                                          {"task_other", ""}},
        },
        SetCriticalActionsConversationIdServiceTestCase{
            .test_name = "AllResolve",
            .actor_task_ids_to_resolve = {"task_1", "task_2", "task_other"},
            .entry_task_ids = {"task_1", "task_2", "task_other"},
            .expected_conversation_ids = {{"task_1", "conv_resolved1"},
                                          {"task_2", "conv_resolved1"},
                                          {"task_other", "conv_resolved1"}},
        },
        SetCriticalActionsConversationIdServiceTestCase{
            .test_name = "NoneResolve",
            .actor_task_ids_to_resolve = {"task_unknown"},
            .entry_task_ids = {"task_1", "task_2", "task_other"},
            .expected_conversation_ids = {{"task_1", ""},
                                          {"task_2", ""},
                                          {"task_other", ""}},
        },
        SetCriticalActionsConversationIdServiceTestCase{
            .test_name = "MultipleActionsSameTask",
            .actor_task_ids_to_resolve = {"task_1"},
            .entry_task_ids = {"task_1", "task_1", "task_other"},
            .expected_conversation_ids = {{"task_1", "conv_resolved1"},
                                          {"task_other", ""}},
        }),
    [](const testing::TestParamInfo<
        SetCriticalActionsConversationIdServiceTestCase>& info) {
      return info.param.test_name;
    });

struct ConversationIdResolutionOutcomeTestCase {
  std::string test_name;
  std::string entry_conversation_id;
  std::string entry_actor_task_id;
  bool populate_cache = false;
  ConversationIdResolutionOutcome expected_outcome;
};

class ConversationIdResolutionOutcomeTest
    : public CriticalActionServiceTest,
      public testing::WithParamInterface<
          ConversationIdResolutionOutcomeTestCase> {};

TEST_P(ConversationIdResolutionOutcomeTest, EmitsExpectedHistogram) {
  const ConversationIdResolutionOutcomeTestCase& test_case = GetParam();
  base::HistogramTester histogram_tester;

  if (test_case.populate_cache) {
    service_->SetCriticalActionsConversationId({test_case.entry_actor_task_id},
                                               "cached_conv_id");
  }

  CriticalActionEntry entry = CreateDefaultEntry();
  entry.conversation_id = test_case.entry_conversation_id;
  entry.actor_task_id = test_case.entry_actor_task_id;

  service_->AddCriticalAction(entry);

  histogram_tester.ExpectUniqueSample(
      "CriticalActions.ConversationIdResolutionOutcome.Actor",
      test_case.expected_outcome, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ConversationIdResolutionOutcomeTest,
    testing::Values(
        ConversationIdResolutionOutcomeTestCase{
            .test_name = "AlreadyPresent",
            .entry_conversation_id = "existing_conv_id",
            .entry_actor_task_id = "task_1",
            .populate_cache = false,
            .expected_outcome =
                ConversationIdResolutionOutcome::kAlreadyPresent,
        },
        ConversationIdResolutionOutcomeTestCase{
            .test_name = "ResolvedFromCache",
            .entry_conversation_id = "",
            .entry_actor_task_id = "task_2",
            .populate_cache = true,
            .expected_outcome =
                ConversationIdResolutionOutcome::kResolvedFromCache,
        },
        ConversationIdResolutionOutcomeTestCase{
            .test_name = "DeferredCacheMiss",
            .entry_conversation_id = "",
            .entry_actor_task_id = "task_3",
            .populate_cache = false,
            .expected_outcome =
                ConversationIdResolutionOutcome::kDeferredCacheMiss,
        },
        ConversationIdResolutionOutcomeTestCase{
            .test_name = "MissingTaskId",
            .entry_conversation_id = "",
            .entry_actor_task_id = "",
            .populate_cache = false,
            .expected_outcome = ConversationIdResolutionOutcome::kMissingTaskId,
        }),
    [](const testing::TestParamInfo<ConversationIdResolutionOutcomeTestCase>&
           info) { return info.param.test_name; });

TEST_F(CriticalActionServiceTest, ConversationIdResolutionOutcomeBackfilled) {
  base::HistogramTester histogram_tester;
  const std::string task_id = "test_task";

  CriticalActionEntry entry1 = CreateDefaultEntry();
  entry1.actor_task_id = task_id;
  service_->AddCriticalAction(entry1);

  CriticalActionEntry entry2 = CreateDefaultEntry();
  entry2.actor_task_id = task_id;
  service_->AddCriticalAction(entry2);

  service_->SetCriticalActionsConversationId({task_id}, "resolved_conv_id");

  histogram_tester.ExpectBucketCount(
      "CriticalActions.ConversationIdResolutionOutcome.Actor",
      ConversationIdResolutionOutcome::kDeferredCacheMiss, 2);
  histogram_tester.ExpectBucketCount(
      "CriticalActions.ConversationIdDeferredResolutionOutcome.Actor",
      ConversationIdDeferredResolutionOutcome::kBackfilled, 2);
}

TEST_F(CriticalActionServiceTest,
       ConversationIdResolutionOutcomeEvictedServiceShutdown) {
  base::HistogramTester histogram_tester;
  const std::string task_id = "test_task";

  CriticalActionEntry entry = CreateDefaultEntry();
  entry.actor_task_id = task_id;
  service_->AddCriticalAction(entry);

  service_->Shutdown();

  histogram_tester.ExpectBucketCount(
      "CriticalActions.ConversationIdResolutionOutcome.Actor",
      ConversationIdResolutionOutcome::kDeferredCacheMiss, 1);
  histogram_tester.ExpectBucketCount(
      "CriticalActions.ConversationIdDeferredResolutionOutcome.Actor",
      ConversationIdDeferredResolutionOutcome::kEvictedServiceShutdown, 1);
}

TEST_F(CriticalActionServiceTest,
       ConversationIdResolutionOutcomeEvictedCapacityExceeded) {
  base::HistogramTester histogram_tester;

  const int max_capacity =
      features::kMaxTaskToCriticalActionIdsCacheCapacity.Get();
  for (int i = 0; i <= max_capacity; ++i) {
    CriticalActionEntry entry = CreateDefaultEntry();
    entry.actor_task_id = base::StrCat({"task_", base::NumberToString(i)});
    service_->AddCriticalAction(entry);
  }

  histogram_tester.ExpectBucketCount(
      "CriticalActions.ConversationIdDeferredResolutionOutcome.Actor",
      ConversationIdDeferredResolutionOutcome::kEvictedCapacityExceeded, 1);
}

}  // namespace critical_actions
