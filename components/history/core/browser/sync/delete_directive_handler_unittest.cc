// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/delete_directive_handler.h"

#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

const std::string kTestAppId1 = "org.chromium.dino.Pteranodon";
const std::string kTestAppId2 = "org.chromium.dino.Trext";

namespace {

base::Time UnixUsecToTime(int64_t usec) {
  return base::Time::UnixEpoch() + base::Microseconds(usec);
}

class TestHistoryBackendDelegate : public HistoryBackend::Delegate {
 public:
  TestHistoryBackendDelegate() = default;

  TestHistoryBackendDelegate(const TestHistoryBackendDelegate&) = delete;
  TestHistoryBackendDelegate& operator=(const TestHistoryBackendDelegate&) =
      delete;

  bool CanAddURL(const GURL& url) const override { return true; }
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {}
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(const URLRow& url_row,
                        const VisitRow& visit_row,
                        std::optional<int64_t> local_navigation_id) override {}
  void NotifyURLsModified(const URLRows& changed_urls) override {}
  void NotifyDeletions(DeletionInfo deletion_info) override {}
  void NotifyVisitedLinksAdded(const HistoryAddPageArgs& args) override {}
  void NotifyVisitedLinksDeleted(
      const std::vector<DeletedVisitedLink>& links) override {}
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const std::u16string& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}
};

void ScheduleDBTask(scoped_refptr<HistoryBackend> history_backend,
                    const base::Location& location,
                    std::unique_ptr<HistoryDBTask> task,
                    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  tracker->NewTrackedTaskId(&is_canceled);
  history_backend->ProcessDBTask(
      std::move(task), base::SingleThreadTaskRunner::GetCurrentDefault(),
      is_canceled);
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

  base::PlatformThread::Sleep(base::Milliseconds(100));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CheckDirectiveProcessingResult, timeout,
                                change_processor, num_changes));
}

class HistoryDeleteDirectiveHandlerTest : public testing::Test {
 public:
  HistoryDeleteDirectiveHandlerTest()
      : history_backend_(base::MakeRefCounted<HistoryBackend>(
            std::make_unique<TestHistoryBackendDelegate>(),
            /*backend_client=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault())) {}

  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    history_backend_->Init(
        false, TestHistoryDatabaseParamsForPath(test_dir_.GetPath()));

    delete_directive_handler_ = std::make_unique<DeleteDirectiveHandler>(
        base::BindRepeating(&ScheduleDBTask, history_backend_));
  }

  void AddPage(const GURL& url,
               base::Time t,
               std::optional<std::string> app_id = kNoAppIdFilter) {
    history::HistoryAddPageArgs args;
    args.url = url;
    args.time = t;
    if (app_id) {
      args.app_id = *app_id;
    }
    history_backend_->AddPage(args);
  }

  QueryURLResult QueryURL(const GURL& url) {
    return history_backend_->QueryURL(url, /*want_visits=*/true);
  }

  HistoryDeleteDirectiveHandlerTest(const HistoryDeleteDirectiveHandlerTest&) =
      delete;
  HistoryDeleteDirectiveHandlerTest& operator=(
      const HistoryDeleteDirectiveHandlerTest&) = delete;

  ~HistoryDeleteDirectiveHandlerTest() override { history_backend_->Closing(); }

  scoped_refptr<HistoryBackend> history_backend() { return history_backend_; }

  DeleteDirectiveHandler* handler() { return delete_directive_handler_.get(); }

  // DeleteDirectiveHandler doesn't actually read the client tag, so a fake
  // constant is used in tests.
  const syncer::ClientTagHash kFakeClientTagHash =
      syncer::ClientTagHash::FromHashed("unused");

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir test_dir_;
  scoped_refptr<HistoryBackend> history_backend_;
  std::unique_ptr<DeleteDirectiveHandler> delete_directive_handler_;
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
      (base::Time::UnixEpoch() + base::Microseconds(1)).ToInternalValue());

  syncer::FakeSyncChangeProcessor change_processor;

  EXPECT_FALSE(
      handler()
          ->MergeDataAndStartSyncing(
              syncer::HISTORY_DELETE_DIRECTIVES, syncer::SyncDataList(),
              std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
                  &change_processor))
          .has_value());

  std::optional<syncer::ModelError> err =
      handler()->ProcessLocalDeleteDirective(delete_directive);
  EXPECT_FALSE(err.has_value());
  EXPECT_EQ(1u, change_processor.changes().size());

  handler()->StopSyncing(syncer::HISTORY_DELETE_DIRECTIVES);
  err = handler()->ProcessLocalDeleteDirective(delete_directive);
  EXPECT_TRUE(err.has_value());
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
      (base::Time::UnixEpoch() + base::Microseconds(6)).ToInternalValue());
  global_id_directive->set_start_time_usec(3);
  global_id_directive->set_end_time_usec(10);
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs, kFakeClientTagHash));

  // 2nd directive.
  global_id_directive->Clear();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::Microseconds(17)).ToInternalValue());
  global_id_directive->set_start_time_usec(13);
  global_id_directive->set_end_time_usec(19);
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs, kFakeClientTagHash));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(handler()
                   ->MergeDataAndStartSyncing(
                       syncer::HISTORY_DELETE_DIRECTIVES, directives,
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               &change_processor)))
                   .has_value());

  // Inject a task to check status and keep message loop filled before directive
  // processing finishes.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CheckDirectiveProcessingResult,
                                base::Time::Now() + base::Seconds(10),
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
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
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
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs, kFakeClientTagHash));

  // 2nd directive.
  time_range_directive->Clear();
  time_range_directive->set_start_time_usec(8);
  time_range_directive->set_end_time_usec(10);
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs, kFakeClientTagHash));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(handler()
                   ->MergeDataAndStartSyncing(
                       syncer::HISTORY_DELETE_DIRECTIVES, directives,
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               &change_processor)))
                   .has_value());

  // Inject a task to check status and keep message loop filled before
  // directive processing finishes.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CheckDirectiveProcessingResult,
                                base::Time::Now() + base::Seconds(10),
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
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
}

// Create delete directives for time ranges with or without app ID.  Only the
// entries with the matching app ID (or all if not specified) should be deleted.
TEST_F(HistoryDeleteDirectiveHandlerTest,
       ProcessTimeRangeDeleteDirectiveByApp) {
  const GURL test_url("http://www.google.com/");
  for (int64_t i = 1; i <= 10; ++i) {
    AddPage(test_url, UnixUsecToTime(i), kTestAppId1);
  }

  {
    QueryURLResult query = QueryURL(test_url);
    ASSERT_TRUE(query.success);
    ASSERT_EQ(10, query.row.visit_count());
  }

  syncer::SyncDataList directives;
  // 1st directive. Entries will be deleted as app ID is not specified, which
  // will match any entries irrespective of the entries' app ID.
  sync_pb::EntitySpecifics entity_specs;
  sync_pb::TimeRangeDirective* time_range_directive =
      entity_specs.mutable_history_delete_directive()
          ->mutable_time_range_directive();
  time_range_directive->set_start_time_usec(2);
  time_range_directive->set_end_time_usec(4);
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs, kFakeClientTagHash));

  // 2nd directive. Entries will be deleted since the app ID is matched.
  time_range_directive->Clear();
  time_range_directive->set_start_time_usec(6);
  time_range_directive->set_end_time_usec(7);
  time_range_directive->set_app_id(kTestAppId1);
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs, kFakeClientTagHash));

  // 3rd directive. Entries cannot be deleted by this directive since the app
  // ID doesn't match.
  time_range_directive->Clear();
  time_range_directive->set_start_time_usec(1);
  time_range_directive->set_end_time_usec(10);
  time_range_directive->set_app_id(kTestAppId2);
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs, kFakeClientTagHash));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(handler()
                   ->MergeDataAndStartSyncing(
                       syncer::HISTORY_DELETE_DIRECTIVES, directives,
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               &change_processor)))
                   .has_value());

  // Inject a task to check status and keep message loop filled before
  // directive processing finishes.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CheckDirectiveProcessingResult,
                                base::Time::Now() + base::Seconds(10),
                                &change_processor, 2));
  base::RunLoop().RunUntilIdle();

  QueryURLResult query = QueryURL(test_url);
  EXPECT_TRUE(query.success);
  ASSERT_EQ(5, query.row.visit_count());
  EXPECT_EQ(UnixUsecToTime(1), query.visits[0].visit_time);
  EXPECT_EQ(UnixUsecToTime(5), query.visits[1].visit_time);
  EXPECT_EQ(UnixUsecToTime(8), query.visits[2].visit_time);
  EXPECT_EQ(UnixUsecToTime(9), query.visits[3].visit_time);
  EXPECT_EQ(UnixUsecToTime(10), query.visits[4].visit_time);

  // Expect three sync changes for deleting processed directives.
  const syncer::SyncChangeList& sync_changes = change_processor.changes();
  ASSERT_EQ(3u, sync_changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[0].change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[2].change_type());
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
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs1, kFakeClientTagHash));
  sync_pb::EntitySpecifics entity_specs2;
  url_directive =
      entity_specs2.mutable_history_delete_directive()->mutable_url_directive();
  url_directive->set_url(test_url2.spec());
  url_directive->set_end_time_usec(8);
  directives.push_back(
      syncer::SyncData::CreateRemoteData(entity_specs2, kFakeClientTagHash));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(handler()
                   ->MergeDataAndStartSyncing(
                       syncer::HISTORY_DELETE_DIRECTIVES, directives,
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               &change_processor)))
                   .has_value());

  // Inject a task to check status and keep message loop filled before
  // directive processing finishes.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CheckDirectiveProcessingResult,
                                base::Time::Now() + base::Seconds(10),
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
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
}

}  // namespace

}  // namespace history
