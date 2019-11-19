// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/delete_directive_handler.h"

#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/model/sync_error_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

base::Time UnixUsecToTime(int64_t usec) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(usec);
}

class TestHistoryBackendDelegate : public HistoryBackend::Delegate {
 public:
  TestHistoryBackendDelegate() {}

  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {}
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override {}
  void NotifyURLsModified(const URLRows& changed_urls) override {}
  void NotifyURLsDeleted(DeletionInfo deletion_info) override {}
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestHistoryBackendDelegate);
};

void ScheduleDBTask(scoped_refptr<HistoryBackend> history_backend,
                    const base::Location& location,
                    std::unique_ptr<HistoryDBTask> task,
                    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  tracker->NewTrackedTaskId(&is_canceled);
  history_backend->ProcessDBTask(
      std::move(task), base::ThreadTaskRunnerHandle::Get(), is_canceled);
}

// Closure function that runs periodically to check result of delete directive
// processing. Stop when timeout or processing ends indicated by the creation
// of sync changes.
void CheckDirectiveProcessingResult(
    base::Time timeout,
    const syncer::FakeSyncChangeProcessor* change_processor,
    uint32_t num_changes) {
  if (base::Time::Now() > timeout ||
      change_processor->changes().size() >= num_changes) {
    return;
  }

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CheckDirectiveProcessingResult, timeout,
                                change_processor, num_changes));
}

class HistoryDeleteDirectiveHandlerTest : public testing::Test {
 public:
  HistoryDeleteDirectiveHandlerTest()
      : history_backend_(base::MakeRefCounted<HistoryBackend>(
            std::make_unique<TestHistoryBackendDelegate>(),
            /*backend_client=*/nullptr,
            base::ThreadTaskRunnerHandle::Get())) {}

  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    history_backend_->Init(
        false, TestHistoryDatabaseParamsForPath(test_dir_.GetPath()));

    delete_directive_handler_ = std::make_unique<DeleteDirectiveHandler>(
        base::BindRepeating(&ScheduleDBTask, history_backend_));
  }

  void AddPage(const GURL& url, base::Time t) {
    history::HistoryAddPageArgs args;
    args.url = url;
    args.time = t;
    history_backend_->AddPage(args);
  }

  QueryURLResult QueryURL(const GURL& url) {
    return history_backend_->QueryURL(url, /*want_visits=*/true);
  }

  ~HistoryDeleteDirectiveHandlerTest() override { history_backend_->Closing(); }

  scoped_refptr<HistoryBackend> history_backend() { return history_backend_; }

  DeleteDirectiveHandler* handler() { return delete_directive_handler_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir test_dir_;
  scoped_refptr<HistoryBackend> history_backend_;
  std::unique_ptr<DeleteDirectiveHandler> delete_directive_handler_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDeleteDirectiveHandlerTest);
};

// Tests calling WaitUntilReadyToSync() after the backend has already been
// loaded, which should report completion immediately.
TEST_F(HistoryDeleteDirectiveHandlerTest, SyncAlreadyReadyToSync) {
  base::MockCallback<base::OnceClosure> ready_cb;
  handler()->OnBackendLoaded();
  EXPECT_CALL(ready_cb, Run());
  handler()->WaitUntilReadyToSync(ready_cb.Get());
}

// Tests calling WaitUntilReadyToSync() befire the backend has been loaded,
// which should only report completion after the backend loading is completed.
TEST_F(HistoryDeleteDirectiveHandlerTest, WaitUntilReadyToSync) {
  base::MockCallback<base::OnceClosure> ready_cb;
  EXPECT_CALL(ready_cb, Run()).Times(0);
  handler()->WaitUntilReadyToSync(ready_cb.Get());
  EXPECT_CALL(ready_cb, Run());
  handler()->OnBackendLoaded();
}

// Create a local delete directive and process it while sync is
// online, and then when offline. The delete directive should be sent to sync,
// no error should be returned for the first time, and an error should be
// returned for the second time.
TEST_F(HistoryDeleteDirectiveHandlerTest,
       ProcessLocalDeleteDirectiveSyncOnline) {
  const GURL test_url("http://www.google.com/");
  for (int64_t i = 1; i <= 10; ++i) {
    AddPage(test_url, UnixUsecToTime(i));
  }

  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
  sync_pb::GlobalIdDirective* global_id_directive =
      delete_directive.mutable_global_id_directive();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(1))
          .ToInternalValue());

  syncer::FakeSyncChangeProcessor change_processor;

  EXPECT_FALSE(
      handler()
          ->MergeDataAndStartSyncing(
              syncer::HISTORY_DELETE_DIRECTIVES, syncer::SyncDataList(),
              std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
                  &change_processor),
              std::unique_ptr<syncer::SyncErrorFactory>())
          .error()
          .IsSet());

  syncer::SyncError err =
      handler()->ProcessLocalDeleteDirective(delete_directive);
  EXPECT_FALSE(err.IsSet());
  EXPECT_EQ(1u, change_processor.changes().size());

  handler()->StopSyncing(syncer::HISTORY_DELETE_DIRECTIVES);
  err = handler()->ProcessLocalDeleteDirective(delete_directive);
  EXPECT_TRUE(err.IsSet());
  EXPECT_EQ(1u, change_processor.changes().size());
}

// Create a delete directive for a few specific history entries,
// including ones that don't exist. The expected entries should be
// deleted.
TEST_F(HistoryDeleteDirectiveHandlerTest, ProcessGlobalIdDeleteDirective) {
  const GURL test_url("http://www.google.com/");
  for (int64_t i = 1; i <= 20; i++) {
    AddPage(test_url, UnixUsecToTime(i));
  }

  {
    QueryURLResult query = QueryURL(test_url);
    EXPECT_TRUE(query.success);
    EXPECT_EQ(20, query.row.visit_count());
  }

  syncer::SyncDataList directives;
  // 1st directive.
  sync_pb::EntitySpecifics entity_specs;
  sync_pb::GlobalIdDirective* global_id_directive =
      entity_specs.mutable_history_delete_directive()
          ->mutable_global_id_directive();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(6))
          .ToInternalValue());
  global_id_directive->set_start_time_usec(3);
  global_id_directive->set_end_time_usec(10);
  directives.push_back(syncer::SyncData::CreateRemoteData(1, entity_specs));

  // 2nd directive.
  global_id_directive->Clear();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(17))
          .ToInternalValue());
  global_id_directive->set_start_time_usec(13);
  global_id_directive->set_end_time_usec(19);
  directives.push_back(syncer::SyncData::CreateRemoteData(2, entity_specs));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(handler()
                   ->MergeDataAndStartSyncing(
                       syncer::HISTORY_DELETE_DIRECTIVES, directives,
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               &change_processor)),
                       std::unique_ptr<syncer::SyncErrorFactory>())
                   .error()
                   .IsSet());

  // Inject a task to check status and keep message loop filled before directive
  // processing finishes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CheckDirectiveProcessingResult,
                     base::Time::Now() + base::TimeDelta::FromSeconds(10),
                     &change_processor, 2));
  base::RunLoop().RunUntilIdle();

  QueryURLResult query = QueryURL(test_url);
  EXPECT_TRUE(query.success);
  ASSERT_EQ(5, query.row.visit_count());
  EXPECT_EQ(UnixUsecToTime(1), query.visits[0].visit_time);
  EXPECT_EQ(UnixUsecToTime(2), query.visits[1].visit_time);
  EXPECT_EQ(UnixUsecToTime(11), query.visits[2].visit_time);
  EXPECT_EQ(UnixUsecToTime(12), query.visits[3].visit_time);
  EXPECT_EQ(UnixUsecToTime(20), query.visits[4].visit_time);

  // Expect two sync changes for deleting processed directives.
  const syncer::SyncChangeList& sync_changes = change_processor.changes();
  ASSERT_EQ(2u, sync_changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[0].change_type());
  EXPECT_EQ(1, syncer::SyncDataRemote(sync_changes[0].sync_data()).GetId());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
  EXPECT_EQ(2, syncer::SyncDataRemote(sync_changes[1].sync_data()).GetId());
}

// Create delete directives for time ranges.  The expected entries should be
// deleted.
TEST_F(HistoryDeleteDirectiveHandlerTest, ProcessTimeRangeDeleteDirective) {
  const GURL test_url("http://www.google.com/");
  for (int64_t i = 1; i <= 10; ++i) {
    AddPage(test_url, UnixUsecToTime(i));
  }

  {
    QueryURLResult query = QueryURL(test_url);
    EXPECT_TRUE(query.success);
    EXPECT_EQ(10, query.row.visit_count());
  }

  syncer::SyncDataList directives;
  // 1st directive.
  sync_pb::EntitySpecifics entity_specs;
  sync_pb::TimeRangeDirective* time_range_directive =
      entity_specs.mutable_history_delete_directive()
          ->mutable_time_range_directive();
  time_range_directive->set_start_time_usec(2);
  time_range_directive->set_end_time_usec(5);
  directives.push_back(syncer::SyncData::CreateRemoteData(1, entity_specs));

  // 2nd directive.
  time_range_directive->Clear();
  time_range_directive->set_start_time_usec(8);
  time_range_directive->set_end_time_usec(10);
  directives.push_back(syncer::SyncData::CreateRemoteData(2, entity_specs));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(handler()
                   ->MergeDataAndStartSyncing(
                       syncer::HISTORY_DELETE_DIRECTIVES, directives,
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               &change_processor)),
                       std::unique_ptr<syncer::SyncErrorFactory>())
                   .error()
                   .IsSet());

  // Inject a task to check status and keep message loop filled before
  // directive processing finishes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CheckDirectiveProcessingResult,
                     base::Time::Now() + base::TimeDelta::FromSeconds(10),
                     &change_processor, 2));
  base::RunLoop().RunUntilIdle();

  QueryURLResult query = QueryURL(test_url);
  EXPECT_TRUE(query.success);
  ASSERT_EQ(3, query.row.visit_count());
  EXPECT_EQ(UnixUsecToTime(1), query.visits[0].visit_time);
  EXPECT_EQ(UnixUsecToTime(6), query.visits[1].visit_time);
  EXPECT_EQ(UnixUsecToTime(7), query.visits[2].visit_time);

  // Expect two sync changes for deleting processed directives.
  const syncer::SyncChangeList& sync_changes = change_processor.changes();
  ASSERT_EQ(2u, sync_changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[0].change_type());
  EXPECT_EQ(1, syncer::SyncDataRemote(sync_changes[0].sync_data()).GetId());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
  EXPECT_EQ(2, syncer::SyncDataRemote(sync_changes[1].sync_data()).GetId());
}

// Create a delete directive for urls.  The expected entries should be
// deleted.
TEST_F(HistoryDeleteDirectiveHandlerTest, ProcessUrlDeleteDirective) {
  const GURL test_url1("http://www.google.com/");
  const GURL test_url2("http://maps.google.com/");

  AddPage(test_url1, UnixUsecToTime(3));
  AddPage(test_url2, UnixUsecToTime(6));
  AddPage(test_url1, UnixUsecToTime(10));

  {
    QueryURLResult query = QueryURL(test_url1);
    EXPECT_TRUE(query.success);
    ASSERT_EQ(2, query.row.visit_count());
    EXPECT_TRUE(QueryURL(test_url2).success);
  }

  // Delete the first visit of url1 and all visits of url2.
  syncer::SyncDataList directives;
  sync_pb::EntitySpecifics entity_specs1;
  sync_pb::UrlDirective* url_directive =
      entity_specs1.mutable_history_delete_directive()->mutable_url_directive();
  url_directive->set_url(test_url1.spec());
  url_directive->set_end_time_usec(8);
  directives.push_back(syncer::SyncData::CreateRemoteData(1, entity_specs1));
  sync_pb::EntitySpecifics entity_specs2;
  url_directive =
      entity_specs2.mutable_history_delete_directive()->mutable_url_directive();
  url_directive->set_url(test_url2.spec());
  url_directive->set_end_time_usec(8);
  directives.push_back(syncer::SyncData::CreateRemoteData(2, entity_specs2));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(handler()
                   ->MergeDataAndStartSyncing(
                       syncer::HISTORY_DELETE_DIRECTIVES, directives,
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               &change_processor)),
                       std::unique_ptr<syncer::SyncErrorFactory>())
                   .error()
                   .IsSet());

  // Inject a task to check status and keep message loop filled before
  // directive processing finishes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CheckDirectiveProcessingResult,
                     base::Time::Now() + base::TimeDelta::FromSeconds(10),
                     &change_processor, 2));
  base::RunLoop().RunUntilIdle();

  QueryURLResult query = QueryURL(test_url1);
  EXPECT_TRUE(query.success);
  EXPECT_EQ(UnixUsecToTime(10), query.visits[0].visit_time);
  EXPECT_FALSE(QueryURL(test_url2).success);

  // Expect a sync change for deleting processed directives.
  const syncer::SyncChangeList& sync_changes = change_processor.changes();
  ASSERT_EQ(2u, sync_changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[0].change_type());
  EXPECT_EQ(1, syncer::SyncDataRemote(sync_changes[0].sync_data()).GetId());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
  EXPECT_EQ(2, syncer::SyncDataRemote(sync_changes[1].sync_data()).GetId());
}

}  // namespace

}  // namespace history
