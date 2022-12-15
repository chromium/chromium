// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::Time AdvanceAndGetTime(base::SimpleTestClock* clock) {
  clock->Advance(base::Milliseconds(10));
  return clock->Now();
}

std::vector<ReadingListEntry> PopulateSampleEntries(
    base::SimpleTestClock* clock) {
  std::vector<ReadingListEntry> entries;
  // Adds timer and interlace read/unread entry creation to avoid having two
  // entries with the same creation timestamp.
  entries.emplace_back(GURL("http://unread_a.com"), "unread_a",
                       AdvanceAndGetTime(clock));

  ReadingListEntry read_a(GURL("http://read_a.com"), "read_a",
                          AdvanceAndGetTime(clock));
  read_a.SetRead(true, AdvanceAndGetTime(clock));
  entries.push_back(std::move(read_a));

  entries.emplace_back(GURL("http://unread_b.com"), "unread_b",
                       AdvanceAndGetTime(clock));

  ReadingListEntry read_b(GURL("http://read_b.com"), "read_b",
                          AdvanceAndGetTime(clock));
  read_b.SetRead(true, AdvanceAndGetTime(clock));
  entries.push_back(std::move(read_b));

  entries.emplace_back(GURL("http://unread_c.com"), "unread_c",
                       AdvanceAndGetTime(clock));

  ReadingListEntry read_c(GURL("http://read_c.com"), "read_c",
                          AdvanceAndGetTime(clock));
  read_c.SetRead(true, AdvanceAndGetTime(clock));
  entries.push_back(std::move(read_c));

  entries.emplace_back(GURL("http://unread_d.com"), "unread_d",
                       AdvanceAndGetTime(clock));

  return entries;
}

class ReadingListModelTest : public ReadingListModelObserver,
                             public FakeReadingListModelStorage::Observer,
                             public testing::Test {
 public:
  ReadingListModelTest() {
    EXPECT_TRUE(ResetStorage()->TriggerLoadCompletion());
  }

  ~ReadingListModelTest() override = default;

  base::WeakPtr<FakeReadingListModelStorage> ResetStorage() {
    model_.reset();
    ClearCounts();

    auto storage =
        std::make_unique<FakeReadingListModelStorage>(/*observer=*/this);
    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();

    model_ =
        std::make_unique<ReadingListModelImpl>(std::move(storage), &clock_);
    model_->AddObserver(this);

    return storage_ptr;
  }

  void ClearCounts() {
    observer_loaded_ = observer_started_batch_update_ =
        observer_completed_batch_update_ = observer_deleted_ =
            observer_remove_ = observer_move_ = observer_add_ =
                observer_did_add_ = observer_update_ = observer_did_update_ =
                    observer_did_apply_ = storage_saved_ = storage_removed_ = 0;
  }

  void AssertObserverCount(int expected_observer_loaded,
                           int expected_observer_started_batch_update,
                           int expected_observer_completed_batch_update,
                           int expected_observer_deleted,
                           int expected_observer_remove,
                           int expected_observer_move,
                           int expected_observer_add,
                           int expected_observer_update,
                           int expected_observer_did_update,
                           int expected_observer_did_apply) {
    ASSERT_EQ(expected_observer_loaded, observer_loaded_);
    ASSERT_EQ(expected_observer_started_batch_update,
              observer_started_batch_update_);
    ASSERT_EQ(expected_observer_completed_batch_update,
              observer_completed_batch_update_);
    ASSERT_EQ(expected_observer_deleted, observer_deleted_);
    ASSERT_EQ(expected_observer_remove, observer_remove_);
    ASSERT_EQ(expected_observer_move, observer_move_);
    // Add and did_add should be the same.
    ASSERT_EQ(expected_observer_add, observer_add_);
    ASSERT_EQ(expected_observer_add, observer_did_add_);
    ASSERT_EQ(expected_observer_update, observer_update_);
    ASSERT_EQ(expected_observer_did_update, observer_did_update_);
    ASSERT_EQ(expected_observer_did_apply, observer_did_apply_);
  }

  void AssertStorageCount(int expected_storage_saved,
                          int expected_storage_removed) {
    ASSERT_EQ(expected_storage_saved, storage_saved_);
    ASSERT_EQ(expected_storage_removed, storage_removed_);
  }

  // ReadingListModelObserver
  void ReadingListModelLoaded(const ReadingListModel* model) override {
    observer_loaded_ += 1;
  }
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override {
    observer_started_batch_update_ += 1;
  }
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override {
    observer_completed_batch_update_ += 1;
  }
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override {
    observer_deleted_ += 1;
  }
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override {
    observer_remove_ += 1;
  }
  void ReadingListWillMoveEntry(const ReadingListModel* model,
                                const GURL& url) override {
    observer_move_ += 1;
  }
  void ReadingListWillAddEntry(const ReadingListModel* model,
                               const ReadingListEntry& entry) override {
    observer_add_ += 1;
  }
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource entry_source) override {
    observer_did_add_ += 1;
  }
  void ReadingListWillUpdateEntry(const ReadingListModel* model,
                                  const GURL& url) override {
    observer_update_ += 1;
  }
  void ReadingListDidUpdateEntry(const ReadingListModel* model,
                                 const GURL& url) override {
    observer_did_update_ += 1;
  }
  void ReadingListDidApplyChanges(ReadingListModel* model) override {
    observer_did_apply_ += 1;
  }

  // FakeReadingListModelStorage::Observer implementation.
  void FakeStorageDidSaveEntry() override { storage_saved_ += 1; }
  void FakeStorageDidRemoveEntry() override { storage_removed_ += 1; }

  size_t UnreadSize() {
    size_t size = 0;
    for (const auto& url : model_->GetKeys()) {
      const ReadingListEntry* entry = model_->GetEntryByURL(url);
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
      const ReadingListEntry* entry = model_->GetEntryByURL(url);
      if (entry->IsRead()) {
        size++;
      }
    }
    return size;
  }

 protected:
  int observer_loaded_;
  int observer_started_batch_update_;
  int observer_completed_batch_update_;
  int observer_deleted_;
  int observer_remove_;
  int observer_move_;
  int observer_add_;
  int observer_did_add_;
  int observer_update_;
  int observer_did_update_;
  int observer_did_apply_;
  int storage_saved_;
  int storage_removed_;

  std::unique_ptr<ReadingListModelImpl> model_;
  base::SimpleTestClock clock_;
};

// Tests creating an empty model.
TEST_F(ReadingListModelTest, EmptyLoaded) {
  EXPECT_TRUE(model_->loaded());
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  model_->Shutdown();
  EXPECT_FALSE(model_->loaded());
  // Shutdown() does not delete the model observer. Verify that deleting the
  // model will delete the model observer.
  model_.reset();
  AssertObserverCount(1, 0, 0, 1, 0, 0, 0, 0, 0, 0);
}

// Tests successful load model.
TEST_F(ReadingListModelTest, ModelLoadSuccess) {
  ASSERT_TRUE(
      ResetStorage()->TriggerLoadCompletion(PopulateSampleEntries(&clock_)));

  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  std::map<GURL, std::string> loaded_entries;
  int size = 0;
  for (const auto& url : model_->GetKeys()) {
    size++;
    const ReadingListEntry* entry = model_->GetEntryByURL(url);
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
  ASSERT_TRUE(
      ResetStorage()->TriggerLoadCompletion(base::unexpected("Fake error")));

  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  EXPECT_TRUE(model_->GetModelTypeSyncBridge()
                  ->change_processor()
                  ->GetError()
                  .has_value());
}

// Tests adding entry.
TEST_F(ReadingListModelTest, AddEntry) {
  ClearCounts();

  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      GURL("http://example.com"), "\n  \tsample Test ",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  EXPECT_EQ(GURL("http://example.com"), entry.URL());
  EXPECT_EQ("sample Test", entry.Title());

  AssertObserverCount(0, 0, 0, 0, 0, 0, 1, 0, 0, 1);
  AssertStorageCount(1, 0);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

  const ReadingListEntry* other_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(GURL("http://example.com"), other_entry->URL());
  EXPECT_EQ("sample Test", other_entry->Title());
}

// Tests adding an entry that already exists.
TEST_F(ReadingListModelTest, AddExistingEntry) {
  GURL url = GURL("http://example.com");
  std::string title = "\n  \tsample Test ";
  model_->AddOrReplaceEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();

  const ReadingListEntry& entry =
      model_->AddOrReplaceEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP,
                                /*estimated_read_time=*/base::TimeDelta());
  EXPECT_EQ(GURL("http://example.com"), entry.URL());
  EXPECT_EQ("sample Test", entry.Title());

  AssertObserverCount(0, 1, 1, 0, 1, 0, 1, 0, 0, 2);
  AssertStorageCount(1, 1);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

  const ReadingListEntry* other_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(GURL("http://example.com"), other_entry->URL());
  EXPECT_EQ("sample Test", other_entry->Title());
}

// Tests addin entry from sync.
TEST_F(ReadingListModelTest, SyncAddEntry) {
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = model_->BeginBatchUpdates();

  auto entry = std::make_unique<ReadingListEntry>(
      GURL("http://example.com"), "sample", AdvanceAndGetTime(&clock_));
  entry->SetRead(true, AdvanceAndGetTime(&clock_));
  ClearCounts();

  model_->SyncAddEntry(std::move(entry));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 1, 0, 0, 1);
  AssertStorageCount(1, 0);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  ClearCounts();
}

// Tests updating entry from sync.
TEST_F(ReadingListModelTest, SyncMergeEntry) {
  model_->AddOrReplaceEntry(GURL("http://example.com"), "sample",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  model_->SetEntryDistilledInfoIfExists(GURL("http://example.com"),
                                        distilled_path, distilled_url, size,
                                        base::Time::FromTimeT(time));
  const ReadingListEntry* local_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  int64_t local_update_time = local_entry->UpdateTime();

  auto sync_entry = std::make_unique<ReadingListEntry>(
      GURL("http://example.com"), "sample", AdvanceAndGetTime(&clock_));
  sync_entry->SetRead(true, AdvanceAndGetTime(&clock_));
  ASSERT_GT(sync_entry->UpdateTime(), local_update_time);
  int64_t sync_update_time = sync_entry->UpdateTime();
  EXPECT_TRUE(sync_entry->DistilledPath().empty());

  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

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

// Tests deleting entry.
TEST_F(ReadingListModelTest, RemoveEntryByUrl) {
  model_->AddOrReplaceEntry(GURL("http://example.com"), "sample",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  model_->RemoveEntryByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);

  model_->AddOrReplaceEntry(GURL("http://example.com"), "sample",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(GURL("http://example.com"), true);
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  model_->RemoveEntryByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
}

// Tests deleting entry from sync.
TEST_F(ReadingListModelTest, RemoveSyncEntryByUrl) {
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = model_->BeginBatchUpdates();
  model_->AddOrReplaceEntry(GURL("http://example.com"), "sample",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  model_->SyncRemoveEntry(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);

  model_->AddOrReplaceEntry(GURL("http://example.com"), "sample",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(GURL("http://example.com"), true);
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  model_->SyncRemoveEntry(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
}

// Tests marking entry read.
TEST_F(ReadingListModelTest, ReadEntry) {
  model_->AddOrReplaceEntry(GURL("http://example.com"), "sample",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  ClearCounts();
  model_->SetReadStatusIfExists(GURL("http://example.com"), true);
  AssertObserverCount(0, 0, 0, 0, 0, 1, 0, 0, 0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  EXPECT_EQ(0ul, model_->unseen_size());

  const ReadingListEntry* other_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  EXPECT_NE(other_entry, nullptr);
  EXPECT_TRUE(other_entry->IsRead());
  EXPECT_EQ(GURL("http://example.com"), other_entry->URL());
  EXPECT_EQ("sample", other_entry->Title());
}

// Tests accessing existing entry.
TEST_F(ReadingListModelTest, EntryFromURL) {
  GURL url1("http://example.com");
  GURL url2("http://example2.com");
  std::string entry1_title = "foo bar qux";
  model_->AddOrReplaceEntry(url1, entry1_title,
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  // Check call with nullptr |read| parameter.
  const ReadingListEntry* entry1 = model_->GetEntryByURL(url1);
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

  const ReadingListEntry* entry2 = model_->GetEntryByURL(url2);
  EXPECT_EQ(nullptr, entry2);
}

// Tests mark entry unread.
TEST_F(ReadingListModelTest, UnreadEntry) {
  // Setup.
  model_->AddOrReplaceEntry(GURL("http://example.com"), "sample",
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(GURL("http://example.com"), true);
  ClearCounts();
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());

  // Action.
  model_->SetReadStatusIfExists(GURL("http://example.com"), false);

  // Tests.
  AssertObserverCount(0, 0, 0, 0, 0, 1, 0, 0, 0, 1);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

  const ReadingListEntry* other_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(GURL("http://example.com"), other_entry->URL());
  EXPECT_EQ("sample", other_entry->Title());
}

// Tests batch updates observers are called.
TEST_F(ReadingListModelTest, BatchUpdates) {
  auto token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete token.release();
  AssertObserverCount(1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

// Tests batch updates are reentrant.
TEST_F(ReadingListModelTest, BatchUpdatesReentrant) {
  // When two updates happen at the same time, the notification is only sent
  // for beginning of first update and completion of last update.
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());

  auto token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  auto second_token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete token.release();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete second_token.release();
  AssertObserverCount(1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());

  // Consequent updates send notifications.
  auto third_token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 2, 1, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete third_token.release();
  AssertObserverCount(1, 2, 2, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

// Tests setting title on unread entry.
TEST_F(ReadingListModelTest, UpdateEntryTitle) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      gurl, "sample", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();

  model_->SetEntryTitleIfExists(gurl, "ping");
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
  EXPECT_EQ("ping", entry.Title());
}
// Tests setting distillation state on unread entry.
TEST_F(ReadingListModelTest, UpdateEntryDistilledState) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      gurl, "sample", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();

  model_->SetEntryDistilledStateIfExists(gurl, ReadingListEntry::PROCESSING);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSING, entry.DistilledState());
}

// Tests setting distillation info on unread entry.
TEST_F(ReadingListModelTest, UpdateDistilledInfo) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddOrReplaceEntry(
      gurl, "sample", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  ClearCounts();

  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  model_->SetEntryDistilledInfoIfExists(GURL("http://example.com"),
                                        distilled_path, distilled_url, size,
                                        base::Time::FromTimeT(time));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry.DistilledState());
  EXPECT_EQ(distilled_path, entry.DistilledPath());
  EXPECT_EQ(distilled_url, entry.DistilledURL());
  EXPECT_EQ(size, entry.DistillationSize());
  EXPECT_EQ(time * base::Time::kMicrosecondsPerSecond,
            entry.DistillationTime());
}

// Tests setting title on read entry.
TEST_F(ReadingListModelTest, UpdateReadEntryTitle) {
  const GURL gurl("http://example.com");
  model_->AddOrReplaceEntry(gurl, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(gurl, true);
  const ReadingListEntry* entry = model_->GetEntryByURL(gurl);
  ClearCounts();

  model_->SetEntryTitleIfExists(gurl, "ping");
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
  EXPECT_EQ("ping", entry->Title());
}

// Tests setting distillation state on read entry.
TEST_F(ReadingListModelTest, UpdateReadEntryState) {
  const GURL gurl("http://example.com");
  model_->AddOrReplaceEntry(gurl, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(gurl, true);
  const ReadingListEntry* entry = model_->GetEntryByURL(gurl);
  ClearCounts();

  model_->SetEntryDistilledStateIfExists(gurl, ReadingListEntry::PROCESSING);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSING, entry->DistilledState());
}

// Tests setting distillation info on read entry.
TEST_F(ReadingListModelTest, UpdateReadDistilledInfo) {
  const GURL gurl("http://example.com");
  model_->AddOrReplaceEntry(gurl, "sample", reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(gurl, true);
  const ReadingListEntry* entry = model_->GetEntryByURL(gurl);
  ClearCounts();

  const base::FilePath distilled_path(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  model_->SetEntryDistilledInfoIfExists(GURL("http://example.com"),
                                        distilled_path, distilled_url, size,
                                        base::Time::FromTimeT(time));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry->DistilledState());
  EXPECT_EQ(distilled_path, entry->DistilledPath());
  EXPECT_EQ(distilled_url, entry->DistilledURL());
  EXPECT_EQ(size, entry->DistillationSize());
  EXPECT_EQ(time * base::Time::kMicrosecondsPerSecond,
            entry->DistillationTime());
}

// Tests that ReadingListModel calls CallbackModelBeingDeleted when destroyed.
TEST_F(ReadingListModelTest, CallbackModelBeingDeleted) {
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  model_.reset();
  AssertObserverCount(1, 0, 0, 1, 0, 0, 0, 0, 0, 0);
}

// Tests that new line characters and spaces are collapsed in title.
TEST_F(ReadingListModelTest, TestTrimmingTitle) {
  const GURL gurl("http://example.com");
  std::string title = "\n  This\ttitle \n contains new     line \n characters ";
  model_->AddOrReplaceEntry(gurl, title, reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());
  model_->SetReadStatusIfExists(gurl, true);
  const ReadingListEntry* entry = model_->GetEntryByURL(gurl);
  EXPECT_EQ(entry->Title(), "This title contains new line characters");
  model_->SetEntryTitleIfExists(gurl, "test");
  EXPECT_EQ(entry->Title(), "test");
  model_->SetEntryTitleIfExists(gurl, title);
  EXPECT_EQ(entry->Title(), "This title contains new line characters");
}

}  // namespace
