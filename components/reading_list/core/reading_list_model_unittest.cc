// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model.h"

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_clock.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/storage_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::IsNull;
using testing::NotNull;

MATCHER_P(HasUrl, expected_url, "") {
  return arg.URL() == expected_url;
}

base::Time AdvanceAndGetTime(base::SimpleTestClock* clock) {
  clock->Advance(base::Milliseconds(10));
  return clock->Now();
}

std::vector<scoped_refptr<ReadingListEntry>> PopulateSampleEntries(
    base::SimpleTestClock* clock) {
  std::vector<scoped_refptr<ReadingListEntry>> entries;
  // Adds timer and interlace read/unread entry creation to avoid having two
  // entries with the same creation timestamp.
  entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://unread_a.com"), "unread_a", AdvanceAndGetTime(clock)));

  auto read_a = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://read_a.com"), "read_a", AdvanceAndGetTime(clock));
  read_a->SetRead(true, AdvanceAndGetTime(clock));
  entries.push_back(std::move(read_a));

  entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://unread_b.com"), "unread_b", AdvanceAndGetTime(clock)));

  auto read_b = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://read_b.com"), "read_b", AdvanceAndGetTime(clock));
  read_b->SetRead(true, AdvanceAndGetTime(clock));
  entries.push_back(std::move(read_b));

  entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://unread_c.com"), "unread_c", AdvanceAndGetTime(clock)));

  auto read_c = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://read_c.com"), "read_c", AdvanceAndGetTime(clock));
  read_c->SetRead(true, AdvanceAndGetTime(clock));
  entries.push_back(std::move(read_c));

  entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://unread_d.com"), "unread_d", AdvanceAndGetTime(clock)));

  return entries;
}

class ReadingListModelTest : public FakeReadingListModelStorage::Observer,
                             public testing::Test {
 public:
  ReadingListModelTest() {
    EXPECT_TRUE(ResetStorage()->TriggerLoadCompletion());
  }

  ~ReadingListModelTest() override = default;

  base::WeakPtr<FakeReadingListModelStorage> ResetStorage() {
    model_.reset();

    auto storage =
        std::make_unique<FakeReadingListModelStorage>(/*observer=*/this);
    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();

    model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever, &clock_);
    model_->AddObserver(&observer_);

    return storage_ptr;
  }

  bool ResetStorageAndMimicSignedOut(
      std::vector<scoped_refptr<ReadingListEntry>> initial_entries = {}) {
    return ResetStorage()->TriggerLoadCompletion(std::move(initial_entries));
  }

  bool ResetStorageAndMimicSyncEnabled(
      std::vector<scoped_refptr<ReadingListEntry>> initial_syncable_entries =
          {}) {
    base::WeakPtr<FakeReadingListModelStorage> storage = ResetStorage();

    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    state.set_authenticated_account_id(kTestAccountId);
    metadata_batch->SetDataTypeState(state);

    return storage->TriggerLoadCompletion(std::move(initial_syncable_entries),
                                          std::move(metadata_batch));
  }

  void ClearCounts() {
    testing::Mock::VerifyAndClearExpectations(&observer_);
    storage_saved_ = 0;
    storage_removed_ = 0;
  }

  // FakeReadingListModelStorage::Observer implementation.
  void FakeStorageDidSaveEntry() override { storage_saved_ += 1; }
  void FakeStorageDidRemoveEntry() override { storage_removed_ += 1; }

  size_t UnseenSize() {
    size_t size = 0;
    for (const auto& url : model_->GetKeys()) {
      scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
      if (!entry->HasBeenSeen()) {
        size++;
      }
    }
    DCHECK_EQ(size, model_->unseen_size());
    return size;
  }

  size_t UnreadSize() {
    size_t size = 0;
    for (const auto& url : model_->GetKeys()) {
      scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
      if (!entry->IsRead()) {
        size++;
      }
    }
    DCHECK_EQ(size, model_->unread_size());
    return size;
  }

  size_t ReadSize() {
    size_t size = 0;
    for (const auto& url : model_->GetKeys()) {
      scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
      if (entry->IsRead()) {
        size++;
      }
    }
    return size;
  }

 protected:
  const std::string kTestAccountId = "TestAccountId";

  int storage_saved_ = 0;
  int storage_removed_ = 0;

  testing::NiceMock<MockReadingListModelObserver> observer_;
  std::unique_ptr<ReadingListModelImpl> model_;
  base::SimpleTestClock clock_;
};

// Tests creating an empty model.
TEST_F(ReadingListModelTest, EmptyLoaded) {
  EXPECT_CALL(observer_, ReadingListModelLoaded(_)).Times(0);
  base::WeakPtr<FakeReadingListModelStorage> storage = ResetStorage();
  // ReadingListModelLoaded() should only be called upon load completion.
  EXPECT_CALL(observer_, ReadingListModelLoaded(model_.get()));
  EXPECT_TRUE(storage->TriggerLoadCompletion());
  EXPECT_TRUE(model_->loaded());
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
}

// Tests successful load model.
TEST_F(ReadingListModelTest, ModelLoadSuccess) {
  ASSERT_TRUE(
      ResetStorage()->TriggerLoadCompletion(PopulateSampleEntries(&clock_)));

  std::map<GURL, std::string> loaded_entries;
  int size = 0;
  for (const auto& url : model_->GetKeys()) {
    size++;
    scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
    loaded_entries[url] = entry->Title();
  }
  EXPECT_EQ(size, 7);
  EXPECT_EQ(loaded_entries[GURL("http://unread_a.com")], "unread_a");
  EXPECT_EQ(loaded_entries[GURL("http://unread_b.com")], "unread_b");
  EXPECT_EQ(loaded_entries[GURL("http://unread_c.com")], "unread_c");
  EXPECT_EQ(loaded_entries[GURL("http://unread_d.com")], "unread_d");
  EXPECT_EQ(loaded_entries[GURL("http://read_a.com")], "read_a");
  EXPECT_EQ(loaded_entries[GURL("http://read_b.com")], "read_b");
  EXPECT_EQ(loaded_entries[GURL("http://read_c.com")], "read_c");
}

// Tests errors during load model.
TEST_F(ReadingListModelTest, ModelLoadFailure) {
  EXPECT_CALL(observer_, ReadingListModelLoaded(_)).Times(0);
  ASSERT_TRUE(
      ResetStorage()->TriggerLoadCompletion(base::unexpected("Fake error")));

  EXPECT_TRUE(model_->GetSyncBridgeForTest()
                  ->change_processor()
                  ->GetError()
                  .has_value());
}

// Tests the model's behavior during shutdown and destruction.
TEST_F(ReadingListModelTest, Shutdown) {
  ASSERT_TRUE(model_->loaded());

  // Shutdown() causes ReadingListModelBeingShutdown().
  EXPECT_CALL(observer_, ReadingListModelBeingShutdown(model_.get()));
  model_->Shutdown();
  EXPECT_FALSE(model_->loaded());

  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Destruction shouldn't notify again, but should instead notify with
  // ReadingListModelBeingDeleted().
  EXPECT_CALL(observer_, ReadingListModelBeingShutdown(_)).Times(0);
  EXPECT_CALL(observer_, ReadingListModelBeingDeleted(model_.get()));
  model_.reset();
}

TEST_F(ReadingListModelTest, MarkEntrySeenIfExists) {
  const GURL example1("http://example1.com/");
  ASSERT_TRUE(ResetStorage()->TriggerLoadCompletion(
      /*entries=*/{base::MakeRefCounted<ReadingListEntry>(
          example1, "example1_title", clock_.Now())}));

  ASSERT_TRUE(model_->loaded());
  ASSERT_FALSE(model_->GetEntryByURL(example1)->HasBeenSeen());
  ASSERT_FALSE(model_->GetEntryByURL(example1)->IsRead());
  ASSERT_EQ(1ul, UnseenSize());
  ASSERT_EQ(1ul, UnreadSize());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), example1));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), example1));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->MarkEntrySeenIfExists(example1);

  EXPECT_TRUE(model_->GetEntryByURL(example1)->HasBeenSeen());
  EXPECT_FALSE(model_->GetEntryByURL(example1)->IsRead());
  EXPECT_EQ(0ul, UnseenSize());
  EXPECT_EQ(1ul, UnreadSize());
}

TEST_F(ReadingListModelTest, MarkAllSeen) {
  const GURL example1("http://example1.com/");
  const GURL example2("http://example2.com/");
  ASSERT_TRUE(ResetStorage()->TriggerLoadCompletion(
      /*entries=*/{base::MakeRefCounted<ReadingListEntry>(
                       example1, "example1_title", clock_.Now()),
                   base::MakeRefCounted<ReadingListEntry>(
                       example2, "example2_title", clock_.Now())}));

  ASSERT_TRUE(model_->loaded());
  ASSERT_FALSE(model_->GetEntryByURL(example1)->HasBeenSeen());
  ASSERT_FALSE(model_->GetEntryByURL(example2)->HasBeenSeen());
  ASSERT_FALSE(model_->GetEntryByURL(example1)->IsRead());
  ASSERT_FALSE(model_->GetEntryByURL(example2)->IsRead());
  ASSERT_EQ(2ul, UnseenSize());
  ASSERT_EQ(2ul, UnreadSize());

  {
    testing::InSequence seq1;
    EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), example1));
    EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), example1));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()))
        .RetiresOnSaturation();
  }

  {
    testing::InSequence seq2;
    EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), example2));
    EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), example2));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()))
        .RetiresOnSaturation();
  }

  model_->MarkAllSeen();

  EXPECT_TRUE(model_->GetEntryByURL(example1)->HasBeenSeen());
  EXPECT_TRUE(model_->GetEntryByURL(example2)->HasBeenSeen());
  EXPECT_FALSE(model_->GetEntryByURL(example1)->IsRead());
  EXPECT_FALSE(model_->GetEntryByURL(example2)->IsRead());
  EXPECT_EQ(0ul, UnseenSize());
  EXPECT_EQ(2ul, UnreadSize());
}

TEST_F(ReadingListModelTest, DeleteAllEntries) {
  const GURL example1("http://example1.com/");
  const GURL example2("http://example2.com/");
  ASSERT_TRUE(ResetStorage()->TriggerLoadCompletion(
      /*entries=*/{base::MakeRefCounted<ReadingListEntry>(
                       example1, "example1_title", clock_.Now()),
                   base::MakeRefCounted<ReadingListEntry>(
                       example2, "example2_title", clock_.Now())}));

  ASSERT_TRUE(model_->loaded());
  ASSERT_THAT(model_->GetEntryByURL(example1), NotNull());
  ASSERT_THAT(model_->GetEntryByURL(example2), NotNull());

  {
    testing::InSequence seq1;
    EXPECT_CALL(observer_, ReadingListWillRemoveEntry(model_.get(), example1));
    EXPECT_CALL(observer_, ReadingListDidRemoveEntry(model_.get(), example1));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()))
        .RetiresOnSaturation();
  }

  {
    testing::InSequence seq2;
    EXPECT_CALL(observer_, ReadingListWillRemoveEntry(model_.get(), example2));
    EXPECT_CALL(observer_, ReadingListDidRemoveEntry(model_.get(), example2));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()))
        .RetiresOnSaturation();
  }

  EXPECT_TRUE(model_->DeleteAllEntries(FROM_HERE));

  EXPECT_THAT(model_->GetEntryByURL(example1), IsNull());
  EXPECT_THAT(model_->GetEntryByURL(example2), IsNull());
}

TEST_F(ReadingListModelTest, GetAccountWhereEntryIsSavedToWhenSignedOut) {
  const GURL example("http://example.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedOut(
      /*initial_entries=*/{base::MakeRefCounted<ReadingListEntry>(
          example, "example_title", clock_.Now())}));

  EXPECT_TRUE(model_->GetAccountWhereEntryIsSavedTo(example).empty());
  EXPECT_TRUE(
      model_
          ->GetAccountWhereEntryIsSavedTo(GURL("http://non_existing_url.com/"))
          .empty());
}

TEST_F(ReadingListModelTest, GetAccountWhereEntryIsSavedToWhenSyncEnabled) {
  const GURL example("http://example.com/");
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled(
      /*initial_syncable_entries=*/{base::MakeRefCounted<ReadingListEntry>(
          example, "example_title", clock_.Now())}));

  EXPECT_EQ(model_->GetAccountWhereEntryIsSavedTo(example).ToString(),
            kTestAccountId);
  EXPECT_TRUE(
      model_
          ->GetAccountWhereEntryIsSavedTo(GURL("http://non_existing_url.com/"))
          .empty());
}

TEST_F(ReadingListModelTest,
       ReadingListModelCompletedBatchUpdatesShouldBeCalledUponSyncEnabled) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled());
  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates);
  model_->GetSyncBridgeForTest()->MergeFullSyncData(
      model_->GetSyncBridgeForTest()->CreateMetadataChangeList(),
      /*syncer::EntityChangeList*/ {});
}

TEST_F(ReadingListModelTest,
       ReadingListModelCompletedBatchUpdatesShouldBeCalledUponSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled());
  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates);
  model_->GetSyncBridgeForTest()->ApplyDisableSyncChanges(
      model_->GetSyncBridgeForTest()->CreateMetadataChangeList());
}

// Tests adding entry.
TEST_F(ReadingListModelTest, AddEntry) {
  const GURL url("http://example.com");

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(_, _)).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillAddEntry(model_.get(), HasUrl(url)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(model_.get(), url,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      url, "\n  \tsample Test ", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_EQ(url, entry.URL());
  EXPECT_EQ("sample Test", entry.Title());

  EXPECT_EQ(1, storage_saved_);
  EXPECT_EQ(0, storage_removed_);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

  scoped_refptr<const ReadingListEntry> other_entry =
      model_->GetEntryByURL(url);
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(url, other_entry->URL());
  EXPECT_EQ("sample Test", other_entry->Title());
}

// Tests adding an entry that already exists.
TEST_F(ReadingListModelTest, AddExistingEntry) {
  const GURL url("http://example.com");
  const std::string title = "\n  \tsample Test ";

  model_->AddOrReplaceEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));
  EXPECT_CALL(observer_, ReadingListWillAddEntry(model_.get(), HasUrl(url)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(model_.get(), url,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  const ReadingListEntry& entry =
      model_->AddOrReplaceEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP,
                                /*estimated_read_time=*/base::TimeDelta());
  EXPECT_EQ(url, entry.URL());
  EXPECT_EQ("sample Test", entry.Title());

  EXPECT_EQ(1, storage_saved_);
  EXPECT_EQ(1, storage_removed_);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

  scoped_refptr<const ReadingListEntry> other_entry =
      model_->GetEntryByURL(url);
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(url, other_entry->URL());
  EXPECT_EQ("sample Test", other_entry->Title());
}

// Tests addin entry from sync.
TEST_F(ReadingListModelTest, SyncAddEntry) {
  const GURL url("http://example.com");

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = model_->BeginBatchUpdates();

  auto entry = base::MakeRefCounted<ReadingListEntry>(
      url, "sample", AdvanceAndGetTime(&clock_));
  entry->SetRead(true, AdvanceAndGetTime(&clock_));
  ClearCounts();

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillAddEntry(model_.get(), HasUrl(url)));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(model_.get(), url,
                                                reading_list::ADDED_VIA_SYNC));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->AddEntry(std::move(entry), reading_list::ADDED_VIA_SYNC);

  EXPECT_EQ(1, storage_saved_);
  EXPECT_EQ(0, storage_removed_);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
}

// Tests updating entry from sync.
TEST_F(ReadingListModelTest, SyncMergeEntry) {
  const GURL url("http://example.com");

  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  model_->SetEntryDistilledInfoIfExists(url, distilled_path, distilled_url,
                                        size, base::Time::FromTimeT(time));
  scoped_refptr<const ReadingListEntry> local_entry =
      model_->GetEntryByURL(url);
  int64_t local_update_time = local_entry->UpdateTime();

  auto sync_entry = base::MakeRefCounted<ReadingListEntry>(
      url, "sample", AdvanceAndGetTime(&clock_));
  sync_entry->SetRead(true, AdvanceAndGetTime(&clock_));
  ASSERT_GT(sync_entry->UpdateTime(), local_update_time);
  int64_t sync_update_time = sync_entry->UpdateTime();
  ASSERT_TRUE(sync_entry->DistilledPath().empty());

  ASSERT_EQ(1ul, UnreadSize());
  ASSERT_EQ(0ul, ReadSize());

  // It's questionable but the current implementation notifies merged entries as
  // moves.
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = model_->BeginBatchUpdates();
  ReadingListEntry* merged_entry =
      model_->SyncMergeEntry(std::move(sync_entry));

  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  EXPECT_EQ(merged_entry->DistilledPath(),
            base::FilePath(FILE_PATH_LITERAL("distilled/page.html")));
  EXPECT_EQ(merged_entry->UpdateTime(), sync_update_time);
  EXPECT_EQ(size, merged_entry->DistillationSize());
  EXPECT_EQ(time * base::Time::kMicrosecondsPerSecond,
            merged_entry->DistillationTime());
}

// Tests deleting entry when the read status is unread.
TEST_F(ReadingListModelTest, RemoveEntryByUrlWhenUnread) {
  const GURL url("http://example.com");
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();
  ASSERT_NE(model_->GetEntryByURL(url), nullptr);
  ASSERT_EQ(1ul, UnreadSize());
  ASSERT_EQ(0ul, ReadSize());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->RemoveEntryByURL(url, FROM_HERE);

  EXPECT_EQ(0, storage_saved_);
  EXPECT_EQ(1, storage_removed_);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(url), nullptr);
}

// Tests deleting entry when the read status is read.
TEST_F(ReadingListModelTest, RemoveEntryByUrlWhenRead) {
  const GURL url("http://example.com");
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(url, true);
  ClearCounts();
  ASSERT_NE(model_->GetEntryByURL(url), nullptr);
  ASSERT_EQ(0ul, UnreadSize());
  ASSERT_EQ(1ul, ReadSize());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->RemoveEntryByURL(url, FROM_HERE);

  EXPECT_EQ(0, storage_saved_);
  EXPECT_EQ(1, storage_removed_);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(url), nullptr);
}

// Tests deleting entry from sync when the read status is unread.
TEST_F(ReadingListModelTest, RemoveSyncEntryByUrlWhenUnread) {
  const GURL url("http://example.com");
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = model_->BeginBatchUpdates();
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();
  ASSERT_NE(model_->GetEntryByURL(url), nullptr);
  ASSERT_EQ(1ul, UnreadSize());
  ASSERT_EQ(0ul, ReadSize());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SyncRemoveEntry(url);

  EXPECT_EQ(0, storage_saved_);
  EXPECT_EQ(1, storage_removed_);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(url), nullptr);
}

// Tests deleting entry from sync when the read status is read.
TEST_F(ReadingListModelTest, RemoveSyncEntryByUrlWhenRead) {
  const GURL url("http://example.com");
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = model_->BeginBatchUpdates();
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(url, true);
  ClearCounts();
  ASSERT_NE(model_->GetEntryByURL(url), nullptr);
  ASSERT_EQ(0ul, UnreadSize());
  ASSERT_EQ(1ul, ReadSize());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SyncRemoveEntry(url);

  EXPECT_EQ(0, storage_saved_);
  EXPECT_EQ(1, storage_removed_);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(url), nullptr);
}

// Tests marking entry read.
TEST_F(ReadingListModelTest, ReadEntry) {
  const GURL url("http://example.com");
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SetReadStatusIfExists(url, true);

  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  EXPECT_EQ(0ul, model_->unseen_size());

  scoped_refptr<const ReadingListEntry> other_entry =
      model_->GetEntryByURL(url);
  EXPECT_NE(other_entry, nullptr);
  EXPECT_TRUE(other_entry->IsRead());
  EXPECT_EQ(url, other_entry->URL());
  EXPECT_EQ("sample", other_entry->Title());
}

// Tests accessing existing entry.
TEST_F(ReadingListModelTest, EntryFromURL) {
  const GURL url1("http://example.com");
  const GURL url2("http://example2.com");
  std::string entry1_title = "foo bar qux";
  model_->AddOrReplaceEntry(url1, entry1_title,
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  // Check call with nullptr |read| parameter.
  scoped_refptr<const ReadingListEntry> entry1 = model_->GetEntryByURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());

  entry1 = model_->GetEntryByURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());
  EXPECT_EQ(entry1->IsRead(), false);
  model_->SetReadStatusIfExists(url1, true);
  entry1 = model_->GetEntryByURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());
  EXPECT_EQ(entry1->IsRead(), true);

  scoped_refptr<const ReadingListEntry> entry2 = model_->GetEntryByURL(url2);
  EXPECT_EQ(nullptr, entry2);
}

// Tests mark entry unread.
TEST_F(ReadingListModelTest, UnreadEntry) {
  const GURL url("http://example.com");
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(url, true);
  ClearCounts();
  ASSERT_EQ(0ul, UnreadSize());
  ASSERT_EQ(1ul, ReadSize());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SetReadStatusIfExists(url, false);

  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

  scoped_refptr<const ReadingListEntry> other_entry =
      model_->GetEntryByURL(url);
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(url, other_entry->URL());
  EXPECT_EQ("sample", other_entry->Title());
}

// Tests batch updates observers are called.
TEST_F(ReadingListModelTest, BatchUpdates) {
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(model_.get()));
  auto token = model_->BeginBatchUpdates();
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates(model_.get()));
  token.reset();
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

// Tests batch updates are reentrant.
TEST_F(ReadingListModelTest, BatchUpdatesReentrant) {
  // ReadingListModelCompletedBatchUpdates() should be invoked at the very end
  // only, and once.
  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates(_)).Times(0);

  EXPECT_FALSE(model_->IsPerformingBatchUpdates());

  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(model_.get()));
  auto token = model_->BeginBatchUpdates();
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // When two updates happen at the same time, the notification is only sent
  // for beginning of first update and completion of last update.
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(_)).Times(0);

  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  auto second_token = model_->BeginBatchUpdates();
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  token.reset();
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates(model_.get()));
  second_token.reset();
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Consequent updates send notifications.
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(model_.get()));
  auto third_token = model_->BeginBatchUpdates();
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates(model_.get()));
  third_token.reset();
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

// Tests setting title on unread entry.
TEST_F(ReadingListModelTest, UpdateEntryTitle) {
  const GURL url("http://example.com");
  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SetEntryTitleIfExists(url, "ping");

  EXPECT_EQ("ping", entry.Title());
}

// Tests setting distillation state on unread entry.
TEST_F(ReadingListModelTest, UpdateEntryDistilledState) {
  const GURL url("http://example.com");
  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SetEntryDistilledStateIfExists(url, ReadingListEntry::PROCESSING);

  EXPECT_EQ(ReadingListEntry::PROCESSING, entry.DistilledState());
}

// Tests setting distillation info on unread entry.
TEST_F(ReadingListModelTest, UpdateDistilledInfo) {
  const GURL url("http://example.com");
  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  model_->SetEntryDistilledInfoIfExists(url, distilled_path, distilled_url,
                                        size, base::Time::FromTimeT(time));

  EXPECT_EQ(ReadingListEntry::PROCESSED, entry.DistilledState());
  EXPECT_EQ(distilled_path, entry.DistilledPath());
  EXPECT_EQ(distilled_url, entry.DistilledURL());
  EXPECT_EQ(size, entry.DistillationSize());
  EXPECT_EQ(time * base::Time::kMicrosecondsPerSecond,
            entry.DistillationTime());
}

// Tests setting title on read entry.
TEST_F(ReadingListModelTest, UpdateReadEntryTitle) {
  const GURL url("http://example.com");
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(url, true);
  scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
  ClearCounts();

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SetEntryTitleIfExists(url, "ping");

  EXPECT_EQ("ping", entry->Title());
}

// Tests setting distillation state on read entry.
TEST_F(ReadingListModelTest, UpdateReadEntryState) {
  const GURL url("http://example.com");
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(url, true);
  scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
  ClearCounts();

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  model_->SetEntryDistilledStateIfExists(url, ReadingListEntry::PROCESSING);

  EXPECT_EQ(ReadingListEntry::PROCESSING, entry->DistilledState());
}

// Tests setting distillation info on read entry.
TEST_F(ReadingListModelTest, UpdateReadDistilledInfo) {
  const GURL url("http://example.com");
  model_->AddOrReplaceEntry(url, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(url, true);
  scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
  ClearCounts();

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(model_.get(), url));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(model_.get()));

  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  model_->SetEntryDistilledInfoIfExists(url, distilled_path, distilled_url,
                                        size, base::Time::FromTimeT(time));

  EXPECT_EQ(ReadingListEntry::PROCESSED, entry->DistilledState());
  EXPECT_EQ(distilled_path, entry->DistilledPath());
  EXPECT_EQ(distilled_url, entry->DistilledURL());
  EXPECT_EQ(size, entry->DistillationSize());
  EXPECT_EQ(time * base::Time::kMicrosecondsPerSecond,
            entry->DistillationTime());
}

// Tests that new line characters and spaces are collapsed in title.
TEST_F(ReadingListModelTest, TestTrimmingTitle) {
  const GURL url("http://example.com");
  std::string title = "\n  This\ttitle \n contains new     line \n characters ";
  model_->AddOrReplaceEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(url, true);
  scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
  EXPECT_EQ(entry->Title(), "This title contains new line characters");
  model_->SetEntryTitleIfExists(url, "test");
  EXPECT_EQ(entry->Title(), "test");
  model_->SetEntryTitleIfExists(url, title);
  EXPECT_EQ(entry->Title(), "This title contains new line characters");
}

}  // namespace
