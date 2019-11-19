// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_store.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

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

// Tests that the transition from |entryA| to |entryB| is possible (|possible|
// is true) or not.
void ExpectAB(const sync_pb::ReadingListSpecifics& entryA,
              const sync_pb::ReadingListSpecifics& entryB,
              bool possible) {
  EXPECT_EQ(ReadingListStore::CompareEntriesForSync(entryA, entryB), possible);
  std::unique_ptr<ReadingListEntry> a =
      ReadingListEntry::FromReadingListSpecifics(entryA,
                                                 base::Time::FromTimeT(10));
  std::unique_ptr<ReadingListEntry> b =
      ReadingListEntry::FromReadingListSpecifics(entryB,
                                                 base::Time::FromTimeT(10));
  a->MergeWithEntry(*b);
  std::unique_ptr<sync_pb::ReadingListSpecifics> mergedEntry =
      a->AsReadingListSpecifics();
  if (possible) {
    // If transition is possible, the merge should be B.
    EXPECT_EQ(entryB.SerializeAsString(), mergedEntry->SerializeAsString());
  } else {
    // If transition is not possible, the transition shold be possible to the
    // merged state.
    EXPECT_TRUE(ReadingListStore::CompareEntriesForSync(entryA, *mergedEntry));
    EXPECT_TRUE(ReadingListStore::CompareEntriesForSync(entryB, *mergedEntry));
  }
}

base::Time AdvanceAndGetTime(base::SimpleTestClock* clock) {
  clock->Advance(base::TimeDelta::FromMilliseconds(10));
  return clock->Now();
}

}  // namespace

class FakeModelTypeChangeProcessorObserver {
 public:
  virtual void Put(const std::string& client_tag,
                   std::unique_ptr<syncer::EntityData> entity_data,
                   syncer::MetadataChangeList* metadata_change_list) = 0;

  virtual void Delete(const std::string& client_tag,
                      syncer::MetadataChangeList* metadata_change_list) = 0;
};

class ReadingListStoreTest : public testing::Test,
                             public ReadingListStoreDelegate {
 protected:
  ReadingListStoreTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ClearState();
    reading_list_store_ = std::make_unique<ReadingListStore>(
        syncer::ModelTypeStoreTestUtil::MoveStoreToFactory(std::move(store_)),
        processor_.CreateForwardingProcessor());
    model_ = std::make_unique<ReadingListModelImpl>(nullptr, nullptr, &clock_);
    reading_list_store_->SetReadingListModel(model_.get(), this, &clock_);

    base::RunLoop().RunUntilIdle();
  }

  void AssertCounts(int sync_add_called,
                    int sync_remove_called,
                    int sync_merge_called) {
    EXPECT_EQ(sync_add_called, sync_add_called_);
    EXPECT_EQ(sync_remove_called, sync_remove_called_);
    EXPECT_EQ(sync_merge_called, sync_merge_called_);
  }

  void ClearState() {
    sync_add_called_ = 0;
    sync_remove_called_ = 0;
    sync_merge_called_ = 0;
    sync_added_.clear();
    sync_removed_.clear();
    sync_merged_.clear();
  }

  // These three mathods handle callbacks from a ReadingListStore.
  void StoreLoaded(std::unique_ptr<ReadingListEntries> entries) override {}

  // Handle sync events.
  void SyncAddEntry(std::unique_ptr<ReadingListEntry> entry) override {
    sync_add_called_++;
    sync_added_[entry->URL().spec()] = entry->IsRead();
  }

  void SyncRemoveEntry(const GURL& gurl) override {
    sync_remove_called_++;
    sync_removed_.insert(gurl.spec());
  }

  ReadingListEntry* SyncMergeEntry(
      std::unique_ptr<ReadingListEntry> entry) override {
    sync_merge_called_++;
    sync_merged_[entry->URL().spec()] = entry->IsRead();
    return model_->SyncMergeEntry(std::move(entry));
  }

  // In memory model type store needs to be able to post tasks.
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<ReadingListModelImpl> model_;
  base::SimpleTestClock clock_;
  std::unique_ptr<ReadingListStore> reading_list_store_;

  int sync_add_called_;
  int sync_remove_called_;
  int sync_merge_called_;
  std::map<std::string, bool> sync_added_;
  std::set<std::string> sync_removed_;
  std::map<std::string, bool> sync_merged_;
};

TEST_F(ReadingListStoreTest, CheckEmpties) {
  EXPECT_EQ(0ul, model_->size());
}

TEST_F(ReadingListStoreTest, SaveOneRead) {
  ReadingListEntry entry(GURL("http://read.example.com/"), "read title",
                         AdvanceAndGetTime(&clock_));
  entry.SetRead(true, AdvanceAndGetTime(&clock_));
  AdvanceAndGetTime(&clock_);
  EXPECT_CALL(processor_,
              Put("http://read.example.com/",
                  MatchesSpecifics("read title", "http://read.example.com/",
                                   sync_pb::ReadingListSpecifics::READ),
                  _));
  reading_list_store_->SaveEntry(entry);
  AssertCounts(0, 0, 0);
}

TEST_F(ReadingListStoreTest, SaveOneUnread) {
  ReadingListEntry entry(GURL("http://unread.example.com/"), "unread title",
                         AdvanceAndGetTime(&clock_));
  EXPECT_CALL(processor_,
              Put("http://unread.example.com/",
                  MatchesSpecifics("unread title", "http://unread.example.com/",
                                   sync_pb::ReadingListSpecifics::UNSEEN),
                  _));
  reading_list_store_->SaveEntry(entry);
  AssertCounts(0, 0, 0);
}

TEST_F(ReadingListStoreTest, SyncMergeOneEntry) {
  EXPECT_CALL(processor_, Put(_, _, _)).Times(0);

  syncer::EntityChangeList remote_input;
  ReadingListEntry entry(GURL("http://read.example.com/"), "read title",
                         AdvanceAndGetTime(&clock_));
  entry.SetRead(true, AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry.AsReadingListSpecifics();

  auto data = std::make_unique<syncer::EntityData>();
  *data->specifics.mutable_reading_list() = *specifics;

  remote_input.push_back(syncer::EntityChange::CreateAdd(
      "http://read.example.com/", std::move(data)));

  std::unique_ptr<syncer::MetadataChangeList> metadata_changes(
      reading_list_store_->CreateMetadataChangeList());
  auto error = reading_list_store_->MergeSyncData(std::move(metadata_changes),
                                                  std::move(remote_input));
  AssertCounts(1, 0, 0);
  EXPECT_EQ(sync_added_.size(), 1u);
  EXPECT_EQ(sync_added_.count("http://read.example.com/"), 1u);
  EXPECT_EQ(sync_added_["http://read.example.com/"], true);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneAdd) {
  EXPECT_CALL(processor_, Put(_, _, _)).Times(0);

  ReadingListEntry entry(GURL("http://read.example.com/"), "read title",
                         AdvanceAndGetTime(&clock_));
  entry.SetRead(true, AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry.AsReadingListSpecifics();
  auto data = std::make_unique<syncer::EntityData>();
  *data->specifics.mutable_reading_list() = *specifics;

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://read.example.com/", std::move(data)));
  auto error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(), std::move(add_changes));
  AssertCounts(1, 0, 0);
  EXPECT_EQ(sync_added_.size(), 1u);
  EXPECT_EQ(sync_added_.count("http://read.example.com/"), 1u);
  EXPECT_EQ(sync_added_["http://read.example.com/"], true);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneMerge) {
  AdvanceAndGetTime(&clock_);
  model_->AddEntry(GURL("http://unread.example.com/"), "unread title",
                   reading_list::ADDED_VIA_CURRENT_APP);

  ReadingListEntry new_entry(GURL("http://unread.example.com/"), "unread title",
                             AdvanceAndGetTime(&clock_));
  new_entry.SetRead(true, AdvanceAndGetTime(&clock_));
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      new_entry.AsReadingListSpecifics();
  auto data = std::make_unique<syncer::EntityData>();
  *data->specifics.mutable_reading_list() = *specifics;

  EXPECT_CALL(processor_, Put("http://unread.example.com/", _, _));

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://unread.example.com/", std::move(data)));
  auto error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(), std::move(add_changes));
  AssertCounts(0, 0, 1);
  EXPECT_EQ(sync_merged_.size(), 1u);
  EXPECT_EQ(sync_merged_.count("http://unread.example.com/"), 1u);
  EXPECT_EQ(sync_merged_["http://unread.example.com/"], true);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneIgnored) {
  // Read entry but with unread URL as it must update the other one.
  ReadingListEntry old_entry(GURL("http://unread.example.com/"),
                             "old unread title", AdvanceAndGetTime(&clock_));
  old_entry.SetRead(true, AdvanceAndGetTime(&clock_));

  AdvanceAndGetTime(&clock_);
  model_->AddEntry(GURL("http://unread.example.com/"), "new unread title",
                   reading_list::ADDED_VIA_CURRENT_APP);
  AssertCounts(0, 0, 0);

  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      old_entry.AsReadingListSpecifics();
  auto data = std::make_unique<syncer::EntityData>();
  *data->specifics.mutable_reading_list() = *specifics;

  EXPECT_CALL(processor_, Put("http://unread.example.com/", _, _));

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://unread.example.com/", std::move(data)));
  auto error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(), std::move(add_changes));
  AssertCounts(0, 0, 1);
  EXPECT_EQ(sync_merged_.size(), 1u);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneRemove) {
  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete("http://read.example.com/"));
  auto error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(),
      std::move(delete_changes));
  AssertCounts(0, 1, 0);
  EXPECT_EQ(sync_removed_.size(), 1u);
  EXPECT_EQ(sync_removed_.count("http://read.example.com/"), 1u);
}

TEST_F(ReadingListStoreTest, CompareEntriesForSync) {
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
  EXPECT_FALSE(ReadingListStore::CompareEntriesForSync(entryA, entryB));
  EXPECT_FALSE(ReadingListStore::CompareEntriesForSync(entryB, entryA));
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
