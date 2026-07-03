// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_service.h"

#include <memory>
#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
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

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<CriticalActionService> service_;
};

// Verifies end-to-end integration and that callbacks run on the main thread.
TEST_F(CriticalActionServiceTest, AddAndGetActionRunsOnMainThread) {
  CriticalActionEntry entry;
  entry.critical_action_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.timestamp = base::Time::Now();
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.actor_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.action_type = ActionType::kCredentialAccess;
  entry.url = GURL("https://example.com/oauth");
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
  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.action_type = ActionType::kFormFill;

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
  CriticalActionEntry entry;
  entry.critical_action_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.action_type = ActionType::kCredentialAccess;
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

  CriticalActionEntry entry1;
  entry1.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry1.timestamp = action_time;
  entry1.action_type = ActionType::kCredentialAccess;
  service_->AddCriticalAction(entry1);

  CriticalActionEntry entry2;
  entry2.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry2.timestamp = end_time + base::Minutes(5);
  entry2.action_type = ActionType::kCredentialAccess;
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

  CriticalActionEntry entry1;
  entry1.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry1.visit_id = visit_id_to_delete;
  entry1.action_type = ActionType::kCredentialAccess;
  service_->AddCriticalAction(entry1);

  CriticalActionEntry entry2;
  entry2.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry2.visit_id = visit_id_to_keep;
  entry2.action_type = ActionType::kCredentialAccess;
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
  CriticalActionEntry entry1;
  entry1.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry1.action_type = ActionType::kFormFill;
  service_->AddCriticalAction(entry1);

  CriticalActionEntry entry2;
  entry2.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry2.action_type = ActionType::kDownload;
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

}  // namespace critical_actions
