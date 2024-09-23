// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reading_list/core/reading_list_sync_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/reading_list/core/reading_list_model_storage_impl.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::SizeIs;

constexpr char kCacheGuid[] = "test_cache_guid";

MATCHER_P3(MatchesSpecifics,
           expected_title,
           expected_url,
           expected_status,
           "") {
  const sync_pb::ReadingListSpecifics& specifics =
      arg->specifics.reading_list();
  if (specifics.title() != expected_title) {
    *result_listener << "which has title \"" << specifics.title();
    return false;
  }
  if (specifics.url() != expected_url) {
    *result_listener << "which has URL " << specifics.url();
    return false;
  }
  if (specifics.status() != expected_status) {
    *result_listener << "which has unexpected status";
    return false;
  }
  return true;
}

MATCHER_P2(MatchesEntry, url_matcher, is_read_matcher, "") {
  if (!arg) {
    *result_listener << "which is null";
    return false;
  }
  return testing::SafeMatcherCast<GURL>(url_matcher)
             .MatchAndExplain(arg->URL(), result_listener) &&
         testing::SafeMatcherCast<bool>(is_read_matcher)
             .MatchAndExplain(arg->IsRead(), result_listener);
}

MATCHER_P(DeletionOriginMatchesLocation, expected_location, "") {
  return arg.is_specified() &&
         *arg.GetLocationForTesting() == expected_location;
}

// Tests that the transition from |entryA| to |entryB| is possible (|possible|
// is true) or not.
void ExpectAB(const sync_pb::ReadingListSpecifics& entryA,
              const sync_pb::ReadingListSpecifics& entryB,
              bool possible) {
  ASSERT_TRUE(ReadingListEntry::IsSpecificsValid(entryA));
  ASSERT_TRUE(ReadingListEntry::IsSpecificsValid(entryB));

  EXPECT_EQ(ReadingListSyncBridge::CompareEntriesForSync(entryA, entryB),
            possible);
  scoped_refptr<ReadingListEntry> a =
      ReadingListEntry::FromReadingListValidSpecifics(
          entryA, base::Time::FromTimeT(10));
  scoped_refptr<ReadingListEntry> b =
      ReadingListEntry::FromReadingListValidSpecifics(
          entryB, base::Time::FromTimeT(10));
  a->MergeWithEntry(*b);
  std::unique_ptr<sync_pb::ReadingListSpecifics> mergedEntry =
      a->AsReadingListSpecifics();
  if (possible) {
    // If transition is possible, the merge should be B.
    EXPECT_EQ(entryB.SerializeAsString(), mergedEntry->SerializeAsString());
  } else {
    // If transition is not possible, the transition shold be possible to the
    // merged state.
    EXPECT_TRUE(
        ReadingListSyncBridge::CompareEntriesForSync(entryA, *mergedEntry));
    EXPECT_TRUE(
        ReadingListSyncBridge::CompareEntriesForSync(entryB, *mergedEntry));
  }
}

base::Time AdvanceAndGetTime(base::SimpleTestClock* clock) {
  clock->Advance(base::Milliseconds(10));
  return clock->Now();
}

syncer::DataTypeStore::RecordList ReadAllDataFromDataTypeStore(
    syncer::DataTypeStore* store) {
  syncer::DataTypeStore::RecordList result;
  base::RunLoop loop;
  store->ReadAllData(base::BindLambdaForTesting(
      [&](const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::DataTypeStore::RecordList> records) {
        EXPECT_FALSE(error.has_value()) << error->ToString();
        result = std::move(*records);
        loop.Quit();
      }));
  loop.Run();
  return result;
}

}  // namespace

class ReadingListSyncBridgeTest : public testing::Test {
 protected:
  ReadingListSyncBridgeTest() {
    ResetModelAndBridge(syncer::StorageType::kUnspecified,
                        syncer::WipeModelUponSyncDisabledBehavior::kNever,
                        /*initial_sync_done=*/true);
  }

  void ResetModelAndBridge(syncer::StorageType storage_type,
                           syncer::WipeModelUponSyncDisabledBehavior
                               wipe_model_upon_sync_disabled_behavior,
                           bool initial_sync_done) {
    std::unique_ptr<syncer::DataTypeStore> data_type_store =
        syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
    underlying_in_memory_store_ = data_type_store.get();

    if (initial_sync_done) {
      // Mimic initial sync having been done earlier.
      sync_pb::DataTypeState data_type_state;
      data_type_state.set_cache_guid(kCacheGuid);
      data_type_state.set_initial_sync_state(
          sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

      std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
          underlying_in_memory_store_->CreateWriteBatch();
      write_batch->GetMetadataChangeList()->UpdateDataTypeState(
          data_type_state);
      underlying_in_memory_store_->CommitWriteBatch(std::move(write_batch),
                                                    base::DoNothing());
    }

    model_ = ReadingListModelImpl::BuildNewForTest(
        std::make_unique<ReadingListModelStorageImpl>(
            syncer::DataTypeStoreTestUtil::MoveStoreToFactory(
                std::move(data_type_store))),
        storage_type, wipe_model_upon_sync_disabled_behavior, &clock_,
        processor_.CreateForwardingProcessor());

    // Wait until the model loads.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(model_->loaded());

    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(initial_sync_done));
  }

  ReadingListSyncBridge* bridge() { return model_->GetSyncBridgeForTest(); }

  // In memory data type store needs to be able to post tasks.
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  std::unique_ptr<ReadingListModelImpl> model_;

  // DataTypeStore is owned by |model_|.
  raw_ptr<syncer::DataTypeStore> underlying_in_memory_store_ = nullptr;
};

TEST_F(ReadingListSyncBridgeTest, SaveOneRead) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://read.example.com/"), "read title",
      AdvanceAndGetTime(&clock_));
  entry->SetRead(true, AdvanceAndGetTime(&clock_));
  AdvanceAndGetTime(&clock_);
  EXPECT_CALL(processor_,
              Put("http://read.example.com/",
                  MatchesSpecifics("read title", "http://read.example.com/",
                                   sync_pb::ReadingListSpecifics::READ),
                  _));
  auto batch = model_->BeginBatchUpdatesWithSyncMetadata();
  bridge()->DidAddOrUpdateEntry(*entry, batch->GetSyncMetadataChangeList());
}

TEST_F(ReadingListSyncBridgeTest, SaveOneUnread) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://unread.example.com/"), "unread title",
      AdvanceAndGetTime(&clock_));
  EXPECT_CALL(processor_,
              Put("http://unread.example.com/",
                  MatchesSpecifics("unread title", "http://unread.example.com/",
                                   sync_pb::ReadingListSpecifics::UNSEEN),
                  _));
  auto batch = model_->BeginBatchUpdatesWithSyncMetadata();
  bridge()->DidAddOrUpdateEntry(*entry, batch->GetSyncMetadataChangeList());
}

TEST_F(ReadingListSyncBridgeTest, DeleteOneEntry) {
  const base::Location kLocation = FROM_HERE;
  auto entry = MakeRefCounted<ReadingListEntry>(
      GURL("http://unread.example.com/"), "unread title",
      AdvanceAndGetTime(&clock_));
  EXPECT_CALL(processor_, Delete("http://unread.example.com/",
                                 DeletionOriginMatchesLocation(kLocation), _));
  auto batch = model_->BeginBatchUpdatesWithSyncMetadata();
  bridge()->DidRemoveEntry(*entry, kLocation,
                           batch->GetSyncMetadataChangeList());
}

TEST_F(ReadingListSyncBridgeTest, SyncMergeOneEntry) {
  EXPECT_CALL(processor_, Put(_, _, _)).Times(0);

  syncer::EntityChangeList remote_input;
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://read.example.com/"), "read title",
      AdvanceAndGetTime(&clock_));
  entry->SetRead(true, AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();

  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  remote_input.push_back(syncer::EntityChange::CreateAdd(
      "http://read.example.com/", std::move(data)));

  std::unique_ptr<syncer::MetadataChangeList> metadata_changes(
      bridge()->CreateMetadataChangeList());

  ASSERT_EQ(0ul, model_->size());
  auto error = bridge()->MergeFullSyncData(std::move(metadata_changes),
                                           std::move(remote_input));
  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(1ul, model_->size());
  EXPECT_THAT(model_->GetEntryByURL(GURL("http://read.example.com/")),
              MatchesEntry("http://read.example.com/",
                           /*is_read=*/true));
}

TEST_F(ReadingListSyncBridgeTest, ApplyIncrementalSyncChangesOneAdd) {
  EXPECT_CALL(processor_, Put(_, _, _)).Times(0);

  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://read.example.com/"), "read title",
      AdvanceAndGetTime(&clock_));
  entry->SetRead(true, AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://read.example.com/", std::move(data)));

  ASSERT_EQ(0ul, model_->size());
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(add_changes));
  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(1ul, model_->size());
  EXPECT_THAT(model_->GetEntryByURL(GURL("http://read.example.com/")),
              MatchesEntry("http://read.example.com/",
                           /*is_read=*/true));
}

TEST_F(ReadingListSyncBridgeTest, ApplyIncrementalSyncChangesOneMerge) {
  AdvanceAndGetTime(&clock_);
  model_->AddOrReplaceEntry(GURL("http://unread.example.com/"), "unread title",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  auto new_entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://unread.example.com/"), "unread title",
      AdvanceAndGetTime(&clock_));
  new_entry->SetRead(true, AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      new_entry->AsReadingListSpecifics();
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  // ApplyIncrementalSyncChanges() must *not* result in any Put() calls - that
  // would risk triggering ping-pong between two syncing devices.
  EXPECT_CALL(processor_, Put(_, _, _)).Times(0);

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://unread.example.com/", std::move(data)));

  ASSERT_EQ(1ul, model_->size());
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(add_changes));
  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(1ul, model_->size());
  EXPECT_THAT(model_->GetEntryByURL(GURL("http://unread.example.com/")),
              MatchesEntry("http://unread.example.com/", /*is_read=*/true));
}

TEST_F(ReadingListSyncBridgeTest, ApplyIncrementalSyncChangesOneIgnored) {
  // Read entry but with unread URL as it must update the other one.
  auto old_entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://unread.example.com/"), "old unread title",
      AdvanceAndGetTime(&clock_));
  old_entry->SetRead(true, AdvanceAndGetTime(&clock_));

  AdvanceAndGetTime(&clock_);
  model_->AddOrReplaceEntry(GURL("http://unread.example.com/"),
                            "new unread title",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      old_entry->AsReadingListSpecifics();
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  // ApplyIncrementalSyncChanges() must *not* result in any Put() calls - that
  // would risk triggering ping-pong between two syncing devices.
  EXPECT_CALL(processor_, Put(_, _, _)).Times(0);

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://unread.example.com/", std::move(data)));

  ASSERT_EQ(1ul, model_->size());
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(add_changes));
  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(1ul, model_->size());
  EXPECT_THAT(model_->GetEntryByURL(GURL("http://unread.example.com/")),
              MatchesEntry("http://unread.example.com/", /*is_read=*/false));
}

TEST_F(ReadingListSyncBridgeTest, ApplyIncrementalSyncChangesOneRemove) {
  model_->AddOrReplaceEntry(GURL("http://read.example.com/"), "read title",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete("http://read.example.com/"));

  ASSERT_EQ(1ul, model_->size());
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(delete_changes));
  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(0ul, model_->size());
}

TEST_F(ReadingListSyncBridgeTest, DisableSyncWithUnspecifiedStorage) {
  ResetModelAndBridge(syncer::StorageType::kUnspecified,
                      syncer::WipeModelUponSyncDisabledBehavior::kNever,
                      /*initial_sync_done=*/true);
  model_->AddOrReplaceEntry(GURL("http://read.example.com/"), "read title",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  ASSERT_EQ(1ul, model_->size());
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(1ul, model_->size());
}

TEST_F(ReadingListSyncBridgeTest, DisableSyncWithAccountStorage) {
  ResetModelAndBridge(syncer::StorageType::kAccount,
                      syncer::WipeModelUponSyncDisabledBehavior::kAlways,
                      /*initial_sync_done=*/true);
  model_->AddOrReplaceEntry(GURL("http://read.example.com/"), "read title",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  ASSERT_EQ(1ul, model_->size());
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(0ul, model_->size());
}

TEST_F(ReadingListSyncBridgeTest,
       DisableSyncWithWipingBehaviorAndInitialSyncDone) {
  ResetModelAndBridge(
      syncer::StorageType::kUnspecified,
      syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata,
      /*initial_sync_done=*/true);
  model_->AddOrReplaceEntry(GURL("http://read.example.com/"), "read title",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  ASSERT_EQ(1ul, model_->size());
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(0ul, model_->size());
}

TEST_F(ReadingListSyncBridgeTest,
       DisableSyncWithWipingBehaviorAndInitialSyncNotDone) {
  ResetModelAndBridge(
      syncer::StorageType::kUnspecified,
      syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata,
      /*initial_sync_done=*/false);
  model_->AddOrReplaceEntry(GURL("http://read.example.com/"), "read title",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  ASSERT_EQ(1ul, model_->size());
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(1ul, model_->size());
}

TEST_F(ReadingListSyncBridgeTest, DisableSyncWithAccountStorageAndOrphanData) {
  ResetModelAndBridge(syncer::StorageType::kAccount,
                      syncer::WipeModelUponSyncDisabledBehavior::kAlways,
                      /*initial_sync_done=*/true);

  // Write some orphan or unexpected data directly onto the underlying
  // DataTypeStore, which should be rare but may be possible due to bugs or
  // edge cases.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      underlying_in_memory_store_->CreateWriteBatch();
  write_batch->WriteData("orphan-data-key", "orphan-data-value");
  std::optional<syncer::ModelError> error;
  base::RunLoop loop;
  underlying_in_memory_store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindLambdaForTesting(
          [&loop](const std::optional<syncer::ModelError>& error) {
            EXPECT_FALSE(error.has_value()) << error->ToString();
            loop.Quit();
          }));
  loop.Run();

  ASSERT_THAT(ReadAllDataFromDataTypeStore(underlying_in_memory_store_),
              SizeIs(1));

  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());

  EXPECT_THAT(ReadAllDataFromDataTypeStore(underlying_in_memory_store_),
              SizeIs(0));
}

TEST_F(ReadingListSyncBridgeTest, CompareEntriesForSync) {
  sync_pb::ReadingListSpecifics entryA;
  sync_pb::ReadingListSpecifics entryB;
  entryA.set_entry_id("http://foo.bar/");
  entryB.set_entry_id("http://foo.bar/");
  entryA.set_url("http://foo.bar/");
  entryB.set_url("http://foo.bar/");
  entryA.set_title("Foo Bar");
  entryB.set_title("Foo Bar");
  entryA.set_status(sync_pb::ReadingListSpecifics::UNREAD);
  entryB.set_status(sync_pb::ReadingListSpecifics::UNREAD);
  entryA.set_creation_time_us(10);
  entryB.set_creation_time_us(10);
  entryA.set_estimated_read_time_seconds(420);
  entryB.set_estimated_read_time_seconds(420);
  entryA.set_first_read_time_us(50);
  entryB.set_first_read_time_us(50);
  entryA.set_update_time_us(100);
  entryB.set_update_time_us(100);
  entryA.set_update_title_time_us(110);
  entryB.set_update_title_time_us(110);
  // Equal entries can be submitted.
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, true);

  // Try to update each field.

  // You cannot change the URL of an entry.
  entryA.set_url("http://foo.foo/");
  EXPECT_FALSE(ReadingListSyncBridge::CompareEntriesForSync(entryA, entryB));
  EXPECT_FALSE(ReadingListSyncBridge::CompareEntriesForSync(entryB, entryA));
  entryA.set_url("http://foo.bar/");

  // You can set a title to a title later in alphabetical order if the
  // update_title_time is the same. If a title has been more recently updated,
  // the only possible transition is to this one.
  entryA.set_title("");
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_update_title_time_us(109);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_update_title_time_us(110);

  entryA.set_title("Foo Aar");
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_update_title_time_us(109);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_update_title_time_us(110);

  entryA.set_title("Foo Ba");
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_update_title_time_us(109);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_update_title_time_us(110);

  entryA.set_title("Foo Bas");
  ExpectAB(entryA, entryB, false);
  ExpectAB(entryB, entryA, true);
  entryA.set_update_title_time_us(109);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_update_title_time_us(110);
  entryA.set_title("Foo Bar");

  // Update times.
  entryA.set_creation_time_us(9);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_first_read_time_us(51);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_first_read_time_us(49);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_first_read_time_us(0);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_first_read_time_us(50);
  entryB.set_first_read_time_us(0);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryB.set_first_read_time_us(50);
  entryA.set_creation_time_us(10);
  entryA.set_first_read_time_us(51);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_first_read_time_us(0);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  entryA.set_first_read_time_us(50);

  entryA.set_creation_time_us(11);
  entryA.set_estimated_read_time_seconds(400);
  ExpectAB(entryB, entryA, true);
  ExpectAB(entryA, entryB, false);
  entryA.set_estimated_read_time_seconds(420);
  entryA.set_creation_time_us(10);

  entryA.set_update_time_us(99);
  ExpectAB(entryA, entryB, true);
  ExpectAB(entryB, entryA, false);
  sync_pb::ReadingListSpecifics::ReadingListEntryStatus status_oder[3] = {
      sync_pb::ReadingListSpecifics::UNSEEN,
      sync_pb::ReadingListSpecifics::UNREAD,
      sync_pb::ReadingListSpecifics::READ};
  for (int index_a = 0; index_a < 3; index_a++) {
    entryA.set_status(status_oder[index_a]);
    for (int index_b = 0; index_b < 3; index_b++) {
      entryB.set_status(status_oder[index_b]);
      ExpectAB(entryA, entryB, true);
      ExpectAB(entryB, entryA, false);
    }
  }
  entryA.set_update_time_us(100);
  for (int index_a = 0; index_a < 3; index_a++) {
    entryA.set_status(status_oder[index_a]);
    entryB.set_status(status_oder[index_a]);
    ExpectAB(entryA, entryB, true);
    ExpectAB(entryB, entryA, true);
    for (int index_b = index_a + 1; index_b < 3; index_b++) {
      entryB.set_status(status_oder[index_b]);
      ExpectAB(entryA, entryB, true);
      ExpectAB(entryB, entryA, false);
    }
  }
}

TEST_F(ReadingListSyncBridgeTest, EntityDataShouldBeValid) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  EXPECT_TRUE(bridge()->IsEntityDataValid(data));
}

TEST_F(ReadingListSyncBridgeTest,
       EntityDataShouldBeNotValidIfItHasEmptyEntryId) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  *specifics->mutable_entry_id() = "";
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  EXPECT_FALSE(bridge()->IsEntityDataValid(data));
}

TEST_F(ReadingListSyncBridgeTest,
       EntityDataShouldBeNotValidIfItHasUnequalEntryIdAndUrl) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://EntryUrl.com/"), "example title",
      AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  *specifics->mutable_entry_id() = "http://UnequalEntryIdAndUrl.com/";
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  EXPECT_FALSE(bridge()->IsEntityDataValid(data));
}

TEST_F(ReadingListSyncBridgeTest, EntityDataShouldBeNotValidIfItHasEmptyUrl) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  *specifics->mutable_url() = "";
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  EXPECT_FALSE(bridge()->IsEntityDataValid(data));
}

TEST_F(ReadingListSyncBridgeTest, EntityDataShouldBeNotValidIfItHasInvalidUrl) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  *specifics->mutable_url() = "InvalidUrl";
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  EXPECT_FALSE(bridge()->IsEntityDataValid(data));
}

TEST_F(ReadingListSyncBridgeTest,
       EntityDataShouldBeNotValidIfTitleContainsNonUTF8) {
  auto entry = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://example.com/"), "example title", AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry->AsReadingListSpecifics();
  *specifics->mutable_title() = "\xFC\x9C\xBF\x80\xBF\x80";
  syncer::EntityData data;
  *data.specifics.mutable_reading_list() = *specifics;

  EXPECT_FALSE(bridge()->IsEntityDataValid(data));
}
