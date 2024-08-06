// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/dual_reading_list_model.h"

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace reading_list {
namespace {

using testing::_;
using testing::IsNull;
using testing::NotNull;
using StorageStateForTesting = DualReadingListModel::StorageStateForTesting;

MATCHER_P2(MatchesEntry, url_matcher, title_matcher, "") {
  if (!arg) {
    *result_listener << "which is null";
    return false;
  }
  return testing::SafeMatcherCast<GURL>(url_matcher)
             .MatchAndExplain(arg->URL(), result_listener) &&
         testing::SafeMatcherCast<std::string>(title_matcher)
             .MatchAndExplain(arg->Title(), result_listener);
}

MATCHER_P(HasUrl, expected_url, "") {
  return arg.URL() == expected_url;
}

MATCHER_P3(HasCountersEqual, size, unseen_size, unread_size, "") {
  return arg->size() == size && arg->unseen_size() == unseen_size &&
         arg->unread_size() == unread_size;
}

class TestEntryBuilder {
 public:
  TestEntryBuilder(GURL url, base::Time now) : url_(url), creation_time_(now) {}

  ~TestEntryBuilder() = default;

  TestEntryBuilder& SetTitle(std::string title) {
    title_and_update_time_ = {title, creation_time_};
    return *this;
  }

  TestEntryBuilder& SetTitle(std::string title, base::Time now) {
    title_and_update_time_ = {title, now};
    return *this;
  }

  TestEntryBuilder& SetRead(bool read = true) {
    read_ = read;
    update_read_time_ = creation_time_;
    return *this;
  }

  TestEntryBuilder& SetRead(base::Time now, bool read = true) {
    read_ = read;
    update_read_time_ = now;
    return *this;
  }

  TestEntryBuilder& SetEstimatedReadTime(base::TimeDelta estimated_read_time) {
    estimated_read_time_ = estimated_read_time;
    return *this;
  }

  TestEntryBuilder& SetDistilledState(
      ReadingListEntry::DistillationState distilation_state) {
    distilation_state_ = distilation_state;
    return *this;
  }

  TestEntryBuilder& SetDistilledInfo(const base::FilePath& distilation_path) {
    distilation_path_ = distilation_path;
    return *this;
  }

  scoped_refptr<ReadingListEntry> Build() {
    auto entry = base::MakeRefCounted<ReadingListEntry>(url_, "entry_title",
                                                        creation_time_);
    if (title_and_update_time_.has_value()) {
      entry->SetTitle(title_and_update_time_.value().first,
                      title_and_update_time_.value().second);
    }

    if (update_read_time_.has_value()) {
      entry->SetRead(read_, update_read_time_.value());
    }

    if (estimated_read_time_.has_value()) {
      entry->SetEstimatedReadTime(estimated_read_time_.value());
    }

    if (distilation_state_.has_value()) {
      entry->SetDistilledState(distilation_state_.value());
    }

    if (distilation_path_.has_value()) {
      entry->SetDistilledInfo(distilation_path_.value(),
                              GURL("http://kDistilledURL.com/"), 1,
                              base::Time::FromTimeT(1));
    }

    return entry;
  }

 private:
  const GURL url_;
  const base::Time creation_time_;

  std::optional<std::pair<std::string, base::Time>> title_and_update_time_;
  std::optional<base::Time> update_read_time_;
  bool read_;
  std::optional<base::TimeDelta> estimated_read_time_;
  std::optional<ReadingListEntry::DistillationState> distilation_state_;
  std::optional<base::FilePath> distilation_path_;
};

class DualReadingListModelTest : public testing::Test {
 public:
  DualReadingListModelTest() = default;
  ~DualReadingListModelTest() override = default;

  void ResetStorage() {
    dual_model_.reset();

    auto local_or_syncable_model_storage =
        std::make_unique<FakeReadingListModelStorage>();
    local_or_syncable_model_storage_ptr_ =
        local_or_syncable_model_storage->AsWeakPtr();
    auto local_or_syncable_model = std::make_unique<ReadingListModelImpl>(
        std::move(local_or_syncable_model_storage),
        syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever, &clock_);
    local_or_syncable_model_ptr_ = local_or_syncable_model.get();

    auto account_model_storage =
        std::make_unique<FakeReadingListModelStorage>();
    account_model_storage_ptr_ = account_model_storage->AsWeakPtr();
    auto account_model = std::make_unique<ReadingListModelImpl>(
        std::move(account_model_storage), syncer::StorageType::kAccount,
        syncer::WipeModelUponSyncDisabledBehavior::kAlways, &clock_);
    account_model_ptr_ = account_model.get();

    dual_model_ = std::make_unique<reading_list::DualReadingListModel>(
        std::move(local_or_syncable_model), std::move(account_model));
    dual_model_->AddObserver(&observer_);
  }

  bool ResetStorageAndTriggerLoadCompletion(
      std::vector<TestEntryBuilder> initial_local_or_syncable_entries_builders =
          {},
      std::vector<TestEntryBuilder> initial_account_entries_builders = {}) {
    ResetStorage();

    std::vector<scoped_refptr<ReadingListEntry>>
        initial_local_or_syncable_entries;
    for (auto entry_builder : initial_local_or_syncable_entries_builders) {
      initial_local_or_syncable_entries.push_back(entry_builder.Build());
    }

    std::vector<scoped_refptr<ReadingListEntry>> initial_account_entries;
    for (auto entry_builder : initial_account_entries_builders) {
      initial_account_entries.push_back(entry_builder.Build());
    }

    return local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_local_or_syncable_entries)) &&
           account_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_account_entries));
  }

  bool ResetStorageAndMimicSignedOut(
      std::vector<TestEntryBuilder> initial_local_entries_builders = {}) {
    return ResetStorageAndTriggerLoadCompletion(
        std::move(initial_local_entries_builders),
        /*initial_account_entries_builders=*/{});
  }

  bool TriggerAccountStorageLoadCompletionSignedInSyncDisabled(
      std::vector<TestEntryBuilder> initial_account_entries_builders = {}) {
    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    state.set_authenticated_account_id(kTestAccountId);
    metadata_batch->SetDataTypeState(state);

    std::vector<scoped_refptr<ReadingListEntry>> initial_account_entries;
    for (auto entry_builder : initial_account_entries_builders) {
      initial_account_entries.push_back(entry_builder.Build());
    }

    return account_model_storage_ptr_->TriggerLoadCompletion(
        std::move(initial_account_entries), std::move(metadata_batch));
  }

  bool TriggerStorageLoadCompletionSignedInSyncDisabled(
      std::vector<TestEntryBuilder> initial_local_entries_builders = {},
      std::vector<TestEntryBuilder> initial_account_entries_builders = {}) {
    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    state.set_authenticated_account_id(kTestAccountId);
    metadata_batch->SetDataTypeState(state);

    std::vector<scoped_refptr<ReadingListEntry>> initial_local_entries;
    for (auto entry_builder : initial_local_entries_builders) {
      initial_local_entries.push_back(entry_builder.Build());
    }

    std::vector<scoped_refptr<ReadingListEntry>> initial_account_entries;
    for (auto entry_builder : initial_account_entries_builders) {
      initial_account_entries.push_back(entry_builder.Build());
    }

    return local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_local_entries)) &&
           account_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_account_entries), std::move(metadata_batch));
  }

  bool ResetStorageAndMimicSignedInSyncDisabled(
      std::vector<TestEntryBuilder> initial_local_entries_builders = {},
      std::vector<TestEntryBuilder> initial_account_entries_builders = {}) {
    ResetStorage();
    return TriggerStorageLoadCompletionSignedInSyncDisabled(
        initial_local_entries_builders, initial_account_entries_builders);
  }

  bool ResetStorageAndMimicSyncEnabled(
      std::vector<TestEntryBuilder> initial_syncable_entries_builders = {}) {
    ResetStorage();

    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    state.set_authenticated_account_id(kTestAccountId);
    metadata_batch->SetDataTypeState(state);

    std::vector<scoped_refptr<ReadingListEntry>> initial_syncable_entries;
    for (auto entry_builder : initial_syncable_entries_builders) {
      initial_syncable_entries.push_back(entry_builder.Build());
    }

    return local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_syncable_entries),
               std::move(metadata_batch)) &&
           account_model_storage_ptr_->TriggerLoadCompletion();
  }

 protected:
  const GURL kUrl = GURL("http://url.com/");
  const std::string kTestAccountId = "TestAccountId";
  base::SimpleTestClock clock_;
  testing::NiceMock<MockReadingListModelObserver> observer_;
  base::WeakPtr<FakeReadingListModelStorage>
      local_or_syncable_model_storage_ptr_;
  base::WeakPtr<FakeReadingListModelStorage> account_model_storage_ptr_;
  // Owned by `dual_model_` and guaranteed to exist while `dual_model_` exists.
  raw_ptr<ReadingListModelImpl, DanglingUntriaged> local_or_syncable_model_ptr_;
  raw_ptr<ReadingListModelImpl, DanglingUntriaged> account_model_ptr_;
  std::unique_ptr<reading_list::DualReadingListModel> dual_model_;
};

// Tests creating an empty model.
TEST_F(DualReadingListModelTest, EmptyLoaded) {
  EXPECT_CALL(observer_, ReadingListModelLoaded).Times(0);
  ResetStorage();
  EXPECT_FALSE(dual_model_->loaded());
  ASSERT_TRUE(local_or_syncable_model_storage_ptr_->TriggerLoadCompletion());
  EXPECT_FALSE(dual_model_->loaded());
  // ReadingListModelLoaded() should only be called upon load completion.
  EXPECT_CALL(observer_, ReadingListModelLoaded(dual_model_.get()));
  ASSERT_TRUE(account_model_storage_ptr_->TriggerLoadCompletion());
  EXPECT_TRUE(dual_model_->loaded());
}

// Tests errors during load model.
TEST_F(DualReadingListModelTest, ModelLoadFailure) {
  EXPECT_CALL(observer_, ReadingListModelLoaded).Times(0);
  ResetStorage();
  ASSERT_TRUE(local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
      base::unexpected("Fake error")));
  ASSERT_TRUE(account_model_storage_ptr_->TriggerLoadCompletion());
  EXPECT_FALSE(dual_model_->loaded());
}

TEST_F(DualReadingListModelTest, MetaDataClearedBeforeModelLoaded) {
  ResetStorage();
  static_cast<syncer::ClientTagBasedDataTypeProcessor*>(
      account_model_ptr_->GetSyncBridgeForTest()->change_processor())
      ->ClearMetadataIfStopped();

  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates).Times(0);
  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);
  EXPECT_CALL(observer_, ReadingListModelLoaded);
  TriggerStorageLoadCompletionSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())});

  EXPECT_EQ(0ul, account_model_ptr_->size());
  EXPECT_EQ(0ul, dual_model_->size());
}

TEST_F(DualReadingListModelTest, UpdatesFromSyncBeforeTheLocalModelIsLoaded) {
  ResetStorage();
  TriggerAccountStorageLoadCompletionSignedInSyncDisabled(
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())});

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListWillAddEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidAddEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->SyncRemoveEntry(kUrl);
  account_model_ptr_->AddEntry(TestEntryBuilder(kUrl, clock_.Now()).Build(),
                               reading_list::ADDED_VIA_SYNC);
}

TEST_F(DualReadingListModelTest, ReturnAccountModelSize) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(0ul, local_or_syncable_model_ptr_->size());
  ASSERT_EQ(1ul, account_model_ptr_->size());
  EXPECT_EQ(1ul, dual_model_->size());
}

TEST_F(DualReadingListModelTest, ReturnLocalModelSize) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(1ul, local_or_syncable_model_ptr_->size());
  ASSERT_EQ(0ul, account_model_ptr_->size());
  EXPECT_EQ(1ul, dual_model_->size());
}

TEST_F(DualReadingListModelTest, ReturnKeysSize) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          GURL("https://url1.com"), clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(GURL("https://url2.com"), clock_.Now())}));
  ASSERT_EQ(1ul, local_or_syncable_model_ptr_->size());
  ASSERT_EQ(1ul, account_model_ptr_->size());
  EXPECT_EQ(2ul, dual_model_->size());
}

TEST_F(DualReadingListModelTest, MarkAllSeen) {
  const GURL kLocalUrl("http://local_url.com/");
  const GURL kAccountUrl("http://account_url.com/");
  const GURL kCommonUrl("http://common_url.com/");

  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kLocalUrl, clock_.Now()),
       TestEntryBuilder(kCommonUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountUrl, clock_.Now()),
          TestEntryBuilder(kCommonUrl, clock_.Now())}));
  ASSERT_TRUE(dual_model_->loaded());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_FALSE(dual_model_->GetEntryByURL(kLocalUrl)->HasBeenSeen());
  ASSERT_FALSE(dual_model_->GetEntryByURL(kAccountUrl)->HasBeenSeen());
  ASSERT_FALSE(dual_model_->GetEntryByURL(kCommonUrl)->HasBeenSeen());
  ASSERT_FALSE(dual_model_->GetEntryByURL(kLocalUrl)->IsRead());
  ASSERT_FALSE(dual_model_->GetEntryByURL(kAccountUrl)->IsRead());
  ASSERT_FALSE(dual_model_->GetEntryByURL(kCommonUrl)->IsRead());

  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(dual_model_.get()));

  {
    testing::InSequence seq1;
    EXPECT_CALL(observer_,
                ReadingListWillUpdateEntry(dual_model_.get(), kLocalUrl));
    EXPECT_CALL(observer_,
                ReadingListDidUpdateEntry(dual_model_.get(), kLocalUrl));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()))
        .RetiresOnSaturation();
  }

  {
    testing::InSequence seq2;
    EXPECT_CALL(observer_,
                ReadingListWillUpdateEntry(dual_model_.get(), kAccountUrl));
    EXPECT_CALL(observer_,
                ReadingListDidUpdateEntry(dual_model_.get(), kAccountUrl));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()))
        .RetiresOnSaturation();
  }

  {
    testing::InSequence seq3;
    EXPECT_CALL(observer_,
                ReadingListWillUpdateEntry(dual_model_.get(), kCommonUrl));
    EXPECT_CALL(observer_,
                ReadingListDidUpdateEntry(dual_model_.get(), kCommonUrl));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()))
        .RetiresOnSaturation();
  }

  EXPECT_CALL(observer_,
              ReadingListModelCompletedBatchUpdates(dual_model_.get()));

  dual_model_->MarkAllSeen();

  EXPECT_TRUE(dual_model_->GetEntryByURL(kLocalUrl)->HasBeenSeen());
  EXPECT_TRUE(dual_model_->GetEntryByURL(kAccountUrl)->HasBeenSeen());
  EXPECT_TRUE(dual_model_->GetEntryByURL(kCommonUrl)->HasBeenSeen());
  EXPECT_FALSE(dual_model_->GetEntryByURL(kLocalUrl)->IsRead());
  EXPECT_FALSE(dual_model_->GetEntryByURL(kAccountUrl)->IsRead());
  EXPECT_FALSE(dual_model_->GetEntryByURL(kCommonUrl)->IsRead());
}

// Verifies that ReadingListModelBeganBatchUpdates and
// ReadingListModelCompletedBatchUpdates are not invoked if MarkAllSeen() is a
// no-op.
TEST_F(DualReadingListModelTest, MarkAllSeenWhenAllEntriesHasBeenSeen) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now()).SetRead(false)},
      /*initial_account_entries_builders=*/{}));
  ASSERT_TRUE(dual_model_->loaded());
  ASSERT_TRUE(dual_model_->GetEntryByURL(kUrl)->HasBeenSeen());

  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates).Times(0);
  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates).Times(0);
  dual_model_->MarkAllSeen();
}

// Regression test for crbug.com/1429274: verifies that there is no infinite
// loop if an observer calls MarkAllSeen() when
// ReadingListModelCompletedBatchUpdates() is called.
TEST_F(DualReadingListModelTest,
       NoInfiniteLoopIfMarkallSeenCalledFromBatchUpdatesObserver) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_TRUE(dual_model_->loaded());

  testing::InSequence seq1;
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(dual_model_.get()));
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));
  // Mimic the behavior of one of the observers by calling MarkAllSeen() when
  // ReadingListModelCompletedBatchUpdates() is called.
  EXPECT_CALL(observer_,
              ReadingListModelCompletedBatchUpdates(dual_model_.get()))
      .WillOnce([&]() { dual_model_->MarkAllSeen(); });

  dual_model_->MarkAllSeen();
}

TEST_F(DualReadingListModelTest, BatchUpdates) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(dual_model_.get()));
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> batch =
      dual_model_->BeginBatchUpdates();
  EXPECT_TRUE(dual_model_->IsPerformingBatchUpdates());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_,
              ReadingListModelCompletedBatchUpdates(dual_model_.get()));
  batch.reset();
  EXPECT_FALSE(dual_model_->IsPerformingBatchUpdates());
}

// Tests batch updates are reentrant.
TEST_F(DualReadingListModelTest, BatchUpdatesReentrant) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());
  // ReadingListModelCompletedBatchUpdates() should be invoked at the very end
  // only, and once.
  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates).Times(0);

  EXPECT_FALSE(dual_model_->IsPerformingBatchUpdates());

  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(dual_model_.get()));
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> batch =
      dual_model_->BeginBatchUpdates();
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // When two updates happen at the same time, the notification is only sent
  // for beginning of first update and completion of last update.
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates).Times(0);

  EXPECT_TRUE(dual_model_->IsPerformingBatchUpdates());

  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> second_batch =
      dual_model_->BeginBatchUpdates();
  EXPECT_TRUE(dual_model_->IsPerformingBatchUpdates());

  batch.reset();
  EXPECT_TRUE(dual_model_->IsPerformingBatchUpdates());

  EXPECT_CALL(observer_,
              ReadingListModelCompletedBatchUpdates(dual_model_.get()));
  second_batch.reset();
  EXPECT_FALSE(dual_model_->IsPerformingBatchUpdates());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Consequent updates send notifications.
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(dual_model_.get()));
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> third_batch =
      dual_model_->BeginBatchUpdates();
  EXPECT_TRUE(dual_model_->IsPerformingBatchUpdates());

  EXPECT_CALL(observer_,
              ReadingListModelCompletedBatchUpdates(dual_model_.get()));
  third_batch.reset();
  EXPECT_FALSE(dual_model_->IsPerformingBatchUpdates());
}

TEST_F(DualReadingListModelTest, GetEntryByURL) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(GURL("http://local_url.com/"), clock_.Now())
           .SetTitle("local_entry"),
       TestEntryBuilder(GURL("http://common_url1.com/"), clock_.Now())
           .SetTitle("merged_entry_title_from_local_entry",
                     clock_.Now() + base::Seconds(1))
           .SetDistilledState(ReadingListEntry::DISTILLATION_ERROR),
       TestEntryBuilder(GURL("http://common_url2.com/"), clock_.Now())
           .SetTitle("merged_entry_title_from_local_entry")
           .SetDistilledState(ReadingListEntry::DISTILLATION_ERROR)},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(GURL("http://account_url.com/"), clock_.Now())
              .SetTitle("account_entry"),
          TestEntryBuilder(GURL("http://common_url1.com/"), clock_.Now())
              .SetTitle("merged_entry_title_from_account_entry"),
          TestEntryBuilder(GURL("http://common_url2.com/"), clock_.Now())
              .SetTitle("merged_entry_title_from_account_entry",
                        clock_.Now() + base::Seconds(1))}));

  ASSERT_TRUE(dual_model_->loaded());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(
                GURL("http://local_url.com/")),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(
                GURL("http://account_url.com/")),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(
                GURL("http://common_url1.com/")),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(
                GURL("http://common_url2.com/")),
            StorageStateForTesting::kExistsInBothModels);

  EXPECT_THAT(dual_model_->GetEntryByURL(GURL("http://local_url.com/")),
              MatchesEntry("http://local_url.com/", "local_entry"));
  EXPECT_THAT(dual_model_->GetEntryByURL(GURL("http://account_url.com/")),
              MatchesEntry("http://account_url.com/", "account_entry"));

  scoped_refptr<const ReadingListEntry> merged_entry1 =
      dual_model_->GetEntryByURL(GURL("http://common_url1.com/"));
  scoped_refptr<const ReadingListEntry> merged_entry2 =
      dual_model_->GetEntryByURL(GURL("http://common_url2.com/"));
  // The expected title of the merged entry is the title that was most recently
  // updated.
  EXPECT_THAT(merged_entry1,
              MatchesEntry("http://common_url1.com/",
                           "merged_entry_title_from_local_entry"));
  EXPECT_THAT(merged_entry2,
              MatchesEntry("http://common_url2.com/",
                           "merged_entry_title_from_account_entry"));
  // The DistilledState() should be equal to the local one.
  EXPECT_EQ(merged_entry1->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_EQ(merged_entry2->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
}

TEST_F(DualReadingListModelTest, DeleteAllEntries) {
  const GURL local_url("http://local_url.com/");
  const GURL account_url("http://account_url.com/");
  const GURL common_url("http://common_url.com/");
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(local_url, clock_.Now()),
       TestEntryBuilder(common_url, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(account_url, clock_.Now()),
          TestEntryBuilder(common_url, clock_.Now())}));

  ASSERT_TRUE(dual_model_->loaded());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(local_url),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(account_url),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(common_url),
            StorageStateForTesting::kExistsInBothModels);

  {
    testing::InSequence seq1;
    EXPECT_CALL(observer_,
                ReadingListWillRemoveEntry(dual_model_.get(), local_url));
    EXPECT_CALL(observer_,
                ReadingListDidRemoveEntry(dual_model_.get(), local_url));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()))
        .RetiresOnSaturation();
  }

  {
    testing::InSequence seq2;
    EXPECT_CALL(observer_,
                ReadingListWillRemoveEntry(dual_model_.get(), account_url));
    EXPECT_CALL(observer_,
                ReadingListDidRemoveEntry(dual_model_.get(), account_url));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()))
        .RetiresOnSaturation();
  }

  {
    testing::InSequence seq3;
    EXPECT_CALL(observer_,
                ReadingListWillRemoveEntry(dual_model_.get(), common_url));
    EXPECT_CALL(observer_,
                ReadingListDidRemoveEntry(dual_model_.get(), common_url));
    EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()))
        .RetiresOnSaturation();
  }

  EXPECT_TRUE(dual_model_->DeleteAllEntries(FROM_HERE));

  EXPECT_THAT(dual_model_->GetEntryByURL(local_url), IsNull());
  EXPECT_THAT(dual_model_->GetEntryByURL(account_url), IsNull());
  EXPECT_THAT(dual_model_->GetEntryByURL(common_url), IsNull());
}

TEST_F(DualReadingListModelTest, GetAccountWhereEntryIsSavedToWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut(/*initial_local_entries_builders=*/{
      TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_TRUE(dual_model_->GetAccountWhereEntryIsSavedTo(kUrl).empty());
  EXPECT_TRUE(
      dual_model_
          ->GetAccountWhereEntryIsSavedTo(GURL("http://non_existing_url.com/"))
          .empty());
}

TEST_F(DualReadingListModelTest,
       GetAccountWhereEntryIsSavedToWhenSignedInSyncDisabled) {
  const GURL kLocalUrl("http://local_url.com/");
  const GURL kAccountUrl("http://account_url.com/");
  const GURL kCommonUrl("http://common_url.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/
      {TestEntryBuilder(kLocalUrl, clock_.Now()),
       TestEntryBuilder(kCommonUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountUrl, clock_.Now()),
          TestEntryBuilder(kCommonUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonUrl),
            StorageStateForTesting::kExistsInBothModels);

  EXPECT_TRUE(dual_model_->GetAccountWhereEntryIsSavedTo(kLocalUrl).empty());
  EXPECT_EQ(dual_model_->GetAccountWhereEntryIsSavedTo(kAccountUrl).ToString(),
            kTestAccountId);
  EXPECT_EQ(dual_model_->GetAccountWhereEntryIsSavedTo(kCommonUrl).ToString(),
            kTestAccountId);
  EXPECT_TRUE(
      dual_model_
          ->GetAccountWhereEntryIsSavedTo(GURL("http://non_existing_url.com/"))
          .empty());
}

TEST_F(DualReadingListModelTest, GetAccountWhereEntryIsSavedToWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_EQ(dual_model_->GetAccountWhereEntryIsSavedTo(kUrl).ToString(),
            kTestAccountId);
  EXPECT_TRUE(
      dual_model_
          ->GetAccountWhereEntryIsSavedTo(GURL("http://non_existing_url.com/"))
          .empty());
}

TEST_F(DualReadingListModelTest, NeedsExplicitUploadToSyncServerWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut(/*initial_local_entries_builders=*/{
      TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kUrl));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(
      GURL("http://non_existing_url.com/")));
}

TEST_F(DualReadingListModelTest,
       NeedsExplicitUploadToSyncServerWhenSignedInSyncDisabled) {
  const GURL kLocalURL("http://local_url.com/");
  const GURL kAccountURL("http://account_url.com/");
  const GURL kCommonURL("http://common_url.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kLocalURL,
                                                           clock_.Now()),
                                          TestEntryBuilder(kCommonURL,
                                                           clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountURL, clock_.Now()),
          TestEntryBuilder(kCommonURL, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalURL),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountURL),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonURL),
            StorageStateForTesting::kExistsInBothModels);

  EXPECT_TRUE(dual_model_->NeedsExplicitUploadToSyncServer(kLocalURL));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kAccountURL));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kCommonURL));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(
      GURL("http://non_existing_url.com/")));
}

TEST_F(DualReadingListModelTest,
       NeedsExplicitUploadToSyncServerWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kUrl));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(
      GURL("http://non_existing_url.com/")));
}

TEST_F(DualReadingListModelTest, MarkAllForUploadToSyncServerIfNeeded) {
  const GURL kLocalURL("http://local_url.com/");
  const GURL kAccountURL("http://account_url.com/");

  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kLocalURL,
                                                           clock_.Now())
                                              .SetTitle("local_entry_title")},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountURL, clock_.Now())
              .SetTitle("account_entry_title")}));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalURL),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountURL),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_TRUE(dual_model_->NeedsExplicitUploadToSyncServer(kLocalURL));
  ASSERT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kAccountURL));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListWillAddEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->MarkAllForUploadToSyncServerIfNeeded();

  EXPECT_THAT(dual_model_->GetEntryByURL(kLocalURL),
              MatchesEntry(kLocalURL, "local_entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kAccountURL),
              MatchesEntry(kAccountURL, "account_entry_title"));

  // Although the entry was originally local only, it has been effectively moved
  // when the entry got uploaded.
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalURL),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountURL),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest, RemoveNonExistingEntryByUrl) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveLocalEntryByUrl) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveAccountEntryByUrl) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveCommonEntryByUrl) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveLocalEntryByUrlFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled(
      /*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  local_or_syncable_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveAccountEntryByUrlFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveCommonEntryByUrlFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest, RemoveLocalEntryByUrlFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled(
      /*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->GetLocalOrSyncableModel()->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveCommonEntryByUrlFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->GetLocalOrSyncableModel()->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest, AddEntryWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest, AddEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest, AddEntryWhenSyncEnabled) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest, AddExistingEntryWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut(/*initial_local_entries_builders=*/{
      TestEntryBuilder(kUrl, clock_.Now())}));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest,
       AddLocalExistingEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  // Although the entry was originally local only, it has been effectively moved
  // when the entry got replaced.
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest,
       AddAccountExistingEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest,
       AddCommonExistingEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest, AddExistingEntryWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kUrl, "entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl),
              MatchesEntry(kUrl, "entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest, AddLocalEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                                reading_list::ADDED_VIA_SYNC));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  local_or_syncable_model_ptr_->AddEntry(
      TestEntryBuilder(kUrl, clock_.Now()).Build(),
      reading_list::ADDED_VIA_SYNC);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest, AddAccountEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                                reading_list::ADDED_VIA_SYNC));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->AddEntry(TestEntryBuilder(kUrl, clock_.Now()).Build(),
                               reading_list::ADDED_VIA_SYNC);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest, AddLocalExistingEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->AddEntry(TestEntryBuilder(kUrl, clock_.Now()).Build(),
                               reading_list::ADDED_VIA_SYNC);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
}

TEST_F(DualReadingListModelTest, AddEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest, AddExistingEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
}

TEST_F(DualReadingListModelTest, SyncMergeEntryWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())
              .SetTitle("current_title")
              .SetRead(clock_.Now() + base::Seconds(1))}));

  scoped_refptr<ReadingListEntry> sync_entry =
      TestEntryBuilder(kUrl, clock_.Now())
          .SetTitle("title_comes_from_sync", clock_.Now() + base::Seconds(1))
          .Build();

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  scoped_refptr<const ReadingListEntry> merged_entry =
      local_or_syncable_model_ptr_->SyncMergeEntry(sync_entry);

  EXPECT_THAT(merged_entry, MatchesEntry(kUrl, "title_comes_from_sync"));
  EXPECT_TRUE(merged_entry->IsRead());

  scoped_refptr<const ReadingListEntry> entry =
      dual_model_->GetEntryByURL(kUrl);

  EXPECT_THAT(entry, MatchesEntry(kUrl, "title_comes_from_sync"));
  EXPECT_TRUE(entry->IsRead());
}

TEST_F(DualReadingListModelTest, SyncMergeEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())
              .SetTitle("current_title")
              .SetRead(clock_.Now() + base::Seconds(1))}));

  scoped_refptr<ReadingListEntry> sync_entry =
      TestEntryBuilder(kUrl, clock_.Now())
          .SetTitle("title_comes_from_sync", clock_.Now() + base::Seconds(1))
          .Build();

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  scoped_refptr<const ReadingListEntry> merged_entry =
      account_model_ptr_->SyncMergeEntry(sync_entry);

  EXPECT_THAT(merged_entry, MatchesEntry(kUrl, "title_comes_from_sync"));
  EXPECT_TRUE(merged_entry->IsRead());

  scoped_refptr<const ReadingListEntry> entry =
      dual_model_->GetEntryByURL(kUrl);

  EXPECT_THAT(entry, MatchesEntry(kUrl, "title_comes_from_sync"));
  EXPECT_TRUE(entry->IsRead());
}

TEST_F(DualReadingListModelTest, SetReadStatusIfExistsForNonExistingEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  EXPECT_CALL(observer_, ReadingListWillMoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidMoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetReadStatusIfExists(kUrl, true);
}

TEST_F(DualReadingListModelTest, SetReadStatusIfExistsForLocalEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_TRUE(dual_model_->GetEntryByURL(kUrl)->IsRead());
}

TEST_F(DualReadingListModelTest, SetReadStatusIfExistsForAccountEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_TRUE(dual_model_->GetEntryByURL(kUrl)->IsRead());
}

TEST_F(DualReadingListModelTest, SetReadStatusIfExistsForLocalCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetRead(clock_.Now() + base::Seconds(1))},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  // The expected read state of the merged entry is equal to the state of the
  // entry that was most recently updated, and initially the update time of the
  // entry is equal to the creation time.
  ASSERT_TRUE(dual_model_->GetEntryByURL(kUrl)->IsRead());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_FALSE(dual_model_->GetEntryByURL(kUrl)->IsRead());
  EXPECT_FALSE(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->IsRead());
  EXPECT_FALSE(account_model_ptr_->GetEntryByURL(kUrl)->IsRead());
}

TEST_F(DualReadingListModelTest, SetReadStatusIfExistsForAccountCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())
              .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  // The expected read state of the merged entry is equal to the state of the
  // entry that was most recently updated, and initially the update time of the
  // entry is equal to the creation time.
  ASSERT_TRUE(dual_model_->GetEntryByURL(kUrl)->IsRead());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_FALSE(dual_model_->GetEntryByURL(kUrl)->IsRead());
  EXPECT_FALSE(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->IsRead());
  EXPECT_FALSE(account_model_ptr_->GetEntryByURL(kUrl)->IsRead());
}

TEST_F(DualReadingListModelTest,
       SetReadStatusIfExistsForMergedEntryHasSameStatus) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_FALSE(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->IsRead());
  // The expected read state of the merged entry is equal to the state of the
  // entry that was most recently updated, and initially the update time of the
  // entry is equal to the creation time.
  ASSERT_TRUE(dual_model_->GetEntryByURL(kUrl)->IsRead());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_TRUE(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->IsRead());
}

TEST_F(DualReadingListModelTest, SetEntryTitleIfExistsForNonExistingEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetEntryTitleIfExists(kUrl, "new_title");
}

TEST_F(DualReadingListModelTest, SetEntryTitleIfExistsForLocalEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEntryTitleIfExists(kUrl, "new_title");

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(), "new_title");
}

TEST_F(DualReadingListModelTest,
       SetEntryTitleIfExistsForLocalEntryFromLocalModel) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  local_or_syncable_model_ptr_->SetEntryTitleIfExists(kUrl, "new_title");

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(), "new_title");
}

TEST_F(DualReadingListModelTest, SetEntryTitleIfExistsForAccountEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEntryTitleIfExists(kUrl, "new_title");

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(), "new_title");
}

TEST_F(DualReadingListModelTest, SetEntryTitleIfExistsForLocalCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetTitle("current_title", clock_.Now() + base::Seconds(1))},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetTitle("old_title_will_be_used_again", clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  // The expected title of the merged entry is the title that was most recently
  // updated.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(), "current_title");

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEntryTitleIfExists(kUrl, "old_title_will_be_used_again");

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(),
            "old_title_will_be_used_again");
  EXPECT_EQ(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->Title(),
            "old_title_will_be_used_again");
  EXPECT_EQ(account_model_ptr_->GetEntryByURL(kUrl)->Title(),
            "old_title_will_be_used_again");
}

TEST_F(DualReadingListModelTest, SetEntryTitleIfExistsForAccountCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetTitle("old_title_will_be_used_again", clock_.Now())},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetTitle("current_title", clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  // The expected title of the merged entry is the title that was most recently
  // updated.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(), "current_title");

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEntryTitleIfExists(kUrl, "old_title_will_be_used_again");

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(),
            "old_title_will_be_used_again");
  EXPECT_EQ(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->Title(),
            "old_title_will_be_used_again");
  EXPECT_EQ(account_model_ptr_->GetEntryByURL(kUrl)->Title(),
            "old_title_will_be_used_again");
}

TEST_F(DualReadingListModelTest,
       SetEntryTitleIfExistsForMergedEntryHasSameTitle) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetTitle("entry_title", clock_.Now())},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetTitle("merge_view_title", clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_EQ(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->Title(),
            "entry_title");
  // The expected title of the merged entry is the title that was most recently
  // updated.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->Title(), "merge_view_title");

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetEntryTitleIfExists(kUrl, "merge_view_title");

  EXPECT_EQ(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->Title(),
            "merge_view_title");
}

TEST_F(DualReadingListModelTest,
       SetEstimatedReadTimeIfExistsForNonExistingEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetEstimatedReadTimeIfExists(kUrl, base::Minutes(1));
}

TEST_F(DualReadingListModelTest, SetEstimatedReadTimeIfExistsForLocalEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEstimatedReadTimeIfExists(kUrl, base::Minutes(1));

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(1));
}

TEST_F(DualReadingListModelTest, SetEstimatedReadTimeIfExistsForAccountEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEstimatedReadTimeIfExists(kUrl, base::Minutes(1));

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(1));
}

TEST_F(DualReadingListModelTest,
       SetEstimatedReadTimeIfExistsForLocalCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetEstimatedReadTime(base::Minutes(1))},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  // The estimated read time for a merged entry is the same as the read time of
  // the newest entry, unless that is zero, in which case the local entry's
  // estimated read time is used.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(1));

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEstimatedReadTimeIfExists(kUrl, base::Minutes(2));

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(2));
  EXPECT_EQ(
      local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->EstimatedReadTime(),
      base::Minutes(2));
  EXPECT_EQ(account_model_ptr_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(2));
}

TEST_F(DualReadingListModelTest,
       SetEstimatedReadTimeIfExistsForAccountCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetEstimatedReadTime(base::Minutes(1))},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1))
           .SetEstimatedReadTime(base::Minutes(2))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  // The estimated read time for a merged entry is the same as the read time of
  // the newest entry, unless that is zero, in which case the local entry's
  // estimated read time is used.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(2));

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEstimatedReadTimeIfExists(kUrl, base::Minutes(3));

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(3));
  EXPECT_EQ(
      local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->EstimatedReadTime(),
      base::Minutes(3));
  EXPECT_EQ(account_model_ptr_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(3));
}

TEST_F(DualReadingListModelTest,
       SetEstimatedReadTimeIfExistsForMergedEntryHasSameEstimatedTime) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetEstimatedReadTime(base::Minutes(1))},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1))
           .SetEstimatedReadTime(base::Minutes(2))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_EQ(
      local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->EstimatedReadTime(),
      base::Minutes(1));
  // The estimated read time for a merged entry is the same as the read time of
  // the newest entry, unless that is zero, in which case the local entry's
  // estimated read time is used.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->EstimatedReadTime(),
            base::Minutes(2));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetEstimatedReadTimeIfExists(kUrl, base::Minutes(2));

  EXPECT_EQ(
      local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->EstimatedReadTime(),
      base::Minutes(2));
}

TEST_F(DualReadingListModelTest,
       SetEntryDistilledStateIfExistsForNonExistingEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetEntryDistilledStateIfExists(
      kUrl, ReadingListEntry::DISTILLATION_ERROR);
}

TEST_F(DualReadingListModelTest, SetEntryDistilledStateIfExistsForLocalEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEntryDistilledStateIfExists(
      kUrl, ReadingListEntry::DISTILLATION_ERROR);

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
}

TEST_F(DualReadingListModelTest,
       SetEntryDistilledStateIfExistsForAccountEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEntryDistilledStateIfExists(
      kUrl, ReadingListEntry::DISTILLATION_ERROR);

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
}

TEST_F(DualReadingListModelTest, SetEntryDistilledStateIfExistsForCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetDistilledState(ReadingListEntry::WILL_RETRY)},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  // The expected distilled state of the merged entry is the distilled state of
  // the local entry.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::WILL_RETRY);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->SetEntryDistilledStateIfExists(
      kUrl, ReadingListEntry::DISTILLATION_ERROR);

  EXPECT_EQ(dual_model_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
  EXPECT_EQ(local_or_syncable_model_ptr_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
}

TEST_F(DualReadingListModelTest,
       SetEntryDistilledStateIfExistsForMergedEntryHasSameDistilledState) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetDistilledState(ReadingListEntry::DISTILLATION_ERROR)},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_EQ(account_model_ptr_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::WAITING);
  // The expected distilled state of the merged entry is the distilled state of
  // the local entry.
  ASSERT_EQ(dual_model_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetEntryDistilledStateIfExists(
      kUrl, ReadingListEntry::DISTILLATION_ERROR);

  EXPECT_EQ(account_model_ptr_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::DISTILLATION_ERROR);
}

TEST_F(DualReadingListModelTest,
       SetEntryDistilledInfoIfExistsForNonExistingEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->SetEntryDistilledInfoIfExists(
      kUrl, base::FilePath(FILE_PATH_LITERAL("distilled/page.html")),
      /*kDistilledURL=*/GURL(("http://example.com/distilled")),
      /*kDistillationSize=*/50,
      /*kDistillationTime=*/base::Time::FromTimeT(100));
}

TEST_F(DualReadingListModelTest, SetEntryDistilledInfoIfExistsForLocalEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  const base::FilePath kDistilledPath(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL kDistilledURL("http://url.com/distilled");
  const int64_t kDistillationSize = 50;
  const int64_t kDistillationTime = 100;
  dual_model_->SetEntryDistilledInfoIfExists(
      kUrl, kDistilledPath, kDistilledURL, kDistillationSize,
      base::Time::FromTimeT(kDistillationTime));

  scoped_refptr<const ReadingListEntry> entry =
      dual_model_->GetEntryByURL(kUrl);
  EXPECT_EQ(entry->DistilledState(), ReadingListEntry::PROCESSED);
  EXPECT_EQ(entry->DistilledPath(), kDistilledPath);
  EXPECT_EQ(entry->DistilledURL(), kDistilledURL);
  EXPECT_EQ(entry->DistillationSize(), kDistillationSize);
  EXPECT_EQ(entry->DistillationTime(),
            kDistillationTime * base::Time::kMicrosecondsPerSecond);
}

TEST_F(DualReadingListModelTest, SetEntryDistilledInfoIfExistsForAccountEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  const base::FilePath kDistilledPath(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL kDistilledURL("http://url.com/distilled");
  const int64_t kDistillationSize = 50;
  const int64_t kDistillationTime = 100;
  dual_model_->SetEntryDistilledInfoIfExists(
      kUrl, kDistilledPath, kDistilledURL, kDistillationSize,
      base::Time::FromTimeT(kDistillationTime));

  scoped_refptr<const ReadingListEntry> entry =
      dual_model_->GetEntryByURL(kUrl);
  EXPECT_EQ(entry->DistilledState(), ReadingListEntry::PROCESSED);
  EXPECT_EQ(entry->DistilledPath(), kDistilledPath);
  EXPECT_EQ(entry->DistilledURL(), kDistilledURL);
  EXPECT_EQ(entry->DistillationSize(), kDistillationSize);
  EXPECT_EQ(entry->DistillationTime(),
            kDistillationTime * base::Time::kMicrosecondsPerSecond);
}

TEST_F(DualReadingListModelTest, SetEntryDistilledInfoIfExistsForCommonEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetDistilledInfo(
               base::FilePath(FILE_PATH_LITERAL("old_distilled/page.html")))},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  scoped_refptr<const ReadingListEntry> merged_entry =
      dual_model_->GetEntryByURL(kUrl);
  // The expected distilled info of the merged entry is the distilled info of
  // the local entry.
  ASSERT_EQ(merged_entry->DistilledState(), ReadingListEntry::PROCESSED);
  ASSERT_EQ(merged_entry->DistilledPath(),
            base::FilePath(FILE_PATH_LITERAL("old_distilled/page.html")));

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  const base::FilePath kDistilledPath(FILE_PATH_LITERAL("distilled/page.html"));
  const GURL kDistilledURL("http://url.com/distilled");
  const int64_t kDistillationSize = 50;
  const int64_t kDistillationTime = 100;
  dual_model_->SetEntryDistilledInfoIfExists(
      kUrl, kDistilledPath, kDistilledURL, kDistillationSize,
      base::Time::FromTimeT(kDistillationTime));

  merged_entry = dual_model_->GetEntryByURL(kUrl);
  EXPECT_EQ(merged_entry->DistilledState(), ReadingListEntry::PROCESSED);
  EXPECT_EQ(merged_entry->DistilledPath(), kDistilledPath);
  EXPECT_EQ(merged_entry->DistilledURL(), kDistilledURL);
  EXPECT_EQ(merged_entry->DistillationSize(), kDistillationSize);
  EXPECT_EQ(merged_entry->DistillationTime(),
            kDistillationTime * base::Time::kMicrosecondsPerSecond);

  scoped_refptr<const ReadingListEntry> local_entry =
      local_or_syncable_model_ptr_->GetEntryByURL(kUrl);
  EXPECT_EQ(local_entry->DistilledState(), ReadingListEntry::PROCESSED);
  EXPECT_EQ(local_entry->DistilledPath(), kDistilledPath);
  EXPECT_EQ(local_entry->DistilledURL(), kDistilledURL);
  EXPECT_EQ(local_entry->DistillationSize(), kDistillationSize);
  EXPECT_EQ(local_entry->DistillationTime(),
            kDistillationTime * base::Time::kMicrosecondsPerSecond);
}

TEST_F(DualReadingListModelTest,
       SetEntryDistilledInfoIfExistsForMergedEntryHasSameDistilledPath) {
  const base::FilePath kDistilledPath(FILE_PATH_LITERAL("distilled/page.html"));
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now()).SetDistilledInfo(kDistilledPath)},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_EQ(account_model_ptr_->GetEntryByURL(kUrl)->DistilledState(),
            ReadingListEntry::WAITING);

  scoped_refptr<const ReadingListEntry> entry =
      dual_model_->GetEntryByURL(kUrl);
  // The expected distilled info of the merged entry is the distilled info of
  // the local entry.
  ASSERT_EQ(entry->DistilledState(), ReadingListEntry::PROCESSED);
  ASSERT_EQ(entry->DistilledPath(), kDistilledPath);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  const GURL kDistilledURL("http://url.com/distilled");
  const int64_t kDistillationSize = 50;
  const int64_t kDistillationTime = 100;
  dual_model_->SetEntryDistilledInfoIfExists(
      kUrl, kDistilledPath, kDistilledURL, kDistillationSize,
      base::Time::FromTimeT(kDistillationTime));

  scoped_refptr<const ReadingListEntry> account_entry =
      account_model_ptr_->GetEntryByURL(kUrl);
  EXPECT_EQ(account_entry->DistilledState(), ReadingListEntry::PROCESSED);
  EXPECT_EQ(account_entry->DistilledPath(), kDistilledPath);
  EXPECT_EQ(account_entry->DistilledURL(), kDistilledURL);
  EXPECT_EQ(account_entry->DistillationSize(), kDistillationSize);
  EXPECT_EQ(account_entry->DistillationTime(),
            kDistillationTime * base::Time::kMicrosecondsPerSecond);
}

// Tests that new line characters and spaces are collapsed in title.
TEST_F(DualReadingListModelTest, TestTrimmingTitle) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  std::string title = "\n  This\ttitle \n contains new     line \n characters ";
  dual_model_->AddOrReplaceEntry(kUrl, title,
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());
  scoped_refptr<const ReadingListEntry> entry =
      dual_model_->GetEntryByURL(kUrl);
  EXPECT_EQ(entry->Title(), "This title contains new line characters");

  dual_model_->SetEntryTitleIfExists(kUrl, "test");
  EXPECT_EQ(entry->Title(), "test");

  dual_model_->SetEntryTitleIfExists(kUrl, title);
  EXPECT_EQ(entry->Title(), "This title contains new line characters");
}

TEST_F(DualReadingListModelTest, ShouldMaintainCountsWhenModelLoaded) {
  const GURL kUnseenLocalUrl("http://unseen_local_url.com/");
  const GURL kUnreadLocalUrl("http://unread_local_url.com/");
  const GURL kReadLocalUrl("http://read_local_url.com/");
  const GURL kUnseenAccountUrl("http://unseen_account_url.com/");
  const GURL kUnreadAccountUrl("http://unread_account_url.com/");
  const GURL kReadAccountUrl("http://read_account_url.com/");
  const GURL kUnreadCommonUrl("http://unread_common_url.com/");
  const GURL kReadCommonUrl("http://read_common_url.com/");

  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUnseenLocalUrl, clock_.Now()),
       TestEntryBuilder(kUnreadLocalUrl, clock_.Now()).SetRead(false),
       TestEntryBuilder(kReadLocalUrl, clock_.Now()).SetRead(),
       TestEntryBuilder(kUnreadCommonUrl, clock_.Now()).SetRead(false),
       TestEntryBuilder(kReadCommonUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUnseenAccountUrl, clock_.Now()),
          TestEntryBuilder(kUnreadAccountUrl, clock_.Now()).SetRead(false),
          TestEntryBuilder(kReadAccountUrl, clock_.Now()).SetRead(),
          TestEntryBuilder(kUnreadCommonUrl, clock_.Now()).SetRead(false),
          TestEntryBuilder(kReadCommonUrl, clock_.Now())
              .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_TRUE(dual_model_->loaded());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadCommonUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadCommonUrl),
            StorageStateForTesting::kExistsInBothModels);

  EXPECT_EQ(8ul, dual_model_->size());
  EXPECT_EQ(2ul, dual_model_->unseen_size());
  EXPECT_EQ(5ul, dual_model_->unread_size());
}

TEST_F(DualReadingListModelTest, ShouldMaintainCountsWhenMarkAllSeen) {
  const GURL kUnseenLocalUrl("http://unseen_local_url.com/");
  const GURL kUnreadLocalUrl("http://unread_local_url.com/");
  const GURL kReadLocalUrl("http://read_local_url.com/");
  const GURL kUnseenAccountUrl("http://unseen_account_url.com/");
  const GURL kUnreadAccountUrl("http://unread_account_url.com/");
  const GURL kReadAccountUrl("http://read_account_url.com/");
  const GURL kUnseenCommonUrl("http://unseen_common_url.com/");
  const GURL kUnreadCommonUrl("http://unread_common_url.com/");
  const GURL kReadCommonUrl("http://read_common_url.com/");

  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUnseenLocalUrl, clock_.Now()),
       TestEntryBuilder(kUnreadLocalUrl, clock_.Now()).SetRead(false),
       TestEntryBuilder(kReadLocalUrl, clock_.Now()).SetRead(),
       TestEntryBuilder(kUnseenCommonUrl, clock_.Now()),
       TestEntryBuilder(kUnreadCommonUrl, clock_.Now()).SetRead(false),
       TestEntryBuilder(kReadCommonUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUnseenAccountUrl, clock_.Now()),
          TestEntryBuilder(kUnreadAccountUrl, clock_.Now()).SetRead(false),
          TestEntryBuilder(kReadAccountUrl, clock_.Now()).SetRead(),
          TestEntryBuilder(kUnseenCommonUrl, clock_.Now()),
          TestEntryBuilder(kUnreadCommonUrl, clock_.Now()).SetRead(false),
          TestEntryBuilder(kReadCommonUrl, clock_.Now())
              .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_TRUE(dual_model_->loaded());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadCommonUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadCommonUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_EQ(9ul, dual_model_->size());
  ASSERT_EQ(3ul, dual_model_->unseen_size());
  ASSERT_EQ(6ul, dual_model_->unread_size());

  dual_model_->MarkAllSeen();

  EXPECT_EQ(9ul, dual_model_->size());
  EXPECT_EQ(0ul, dual_model_->unseen_size());
  EXPECT_EQ(6ul, dual_model_->unread_size());
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveLocalUnreadEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest, ShouldMaintainCountsWhenRemoveLocalReadEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveAccountUnreadEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveAccountReadEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveCommonUnreadEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveCommonReadEntry) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())
              .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveLocalUnreadEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled(
      /*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  local_or_syncable_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveLocalReadEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled(
      /*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  local_or_syncable_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveAccountUnreadEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveAccountReadEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveCommonUnreadEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kUrl, clock_.Now())
                                              .SetRead()},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveCommonReadEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())
              .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveLocalUnreadEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->GetLocalOrSyncableModel()->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveLocalReadEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->GetLocalOrSyncableModel()->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveCommonUnreadEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now() + base::Seconds(1))},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->GetLocalOrSyncableModel()->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenRemoveCommonReadEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kUrl, clock_.Now())
                                              .SetRead(clock_.Now() +
                                                       base::Seconds(1))},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));

  dual_model_->GetLocalOrSyncableModel()->RemoveEntryByURL(kUrl, FROM_HERE);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddEntryWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddEntryWhenSyncEnabled) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddExistingUnreadEntryWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut(/*initial_local_entries_builders=*/{
      TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddExistingReadEntryWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut(/*initial_local_entries_builders=*/{
      TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenAddLocalExistingUnreadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenAddLocalExistingReadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenAddAccountExistingUnreadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenAddAccountExistingReadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenAddCommonExistingUnreadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenAddCommonExistingReadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())
              .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddExistingUnreadEntryWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddExistingReadEntryWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->AddOrReplaceEntry(kUrl, "entry_title",
                                 reading_list::ADDED_VIA_CURRENT_APP,
                                 /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddLocalEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  local_or_syncable_model_ptr_->AddEntry(
      TestEntryBuilder(kUrl, clock_.Now()).Build(),
      reading_list::ADDED_VIA_SYNC);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddAccountEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->AddEntry(TestEntryBuilder(kUrl, clock_.Now()).Build(),
                               reading_list::ADDED_VIA_SYNC);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddLocalExistingEntryFromSync) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  account_model_ptr_->AddEntry(
      TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1)).Build(),
      reading_list::ADDED_VIA_SYNC);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddLocalEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled());
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddLocalExistingEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillAddEntry(
                             HasCountersEqual(/*size=*/0ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidAddEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _, _));

  dual_model_->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenAddAccountExistingEntryFromTheLocalModel) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders*/ {
          TestEntryBuilder(kUrl, clock_.Now() - base::Seconds(1)).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));

  dual_model_->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      kUrl, "entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetLocalUnreadEntryUnread) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetLocalUnreadEntryRead) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetLocalReadEntryUnread) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/1ul),
                             _));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetLocalReadEntryRead) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetAccountUnreadEntryUnread) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetAccountUnreadEntryRead) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetAccountReadEntryUnread) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/1ul),
                             _));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetAccountReadEntryRead) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now()).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetCommonUnreadEntryUnread) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetCommonUnreadEntryRead) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetCommonReadEntryUnread) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now()).SetRead()},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/1ul),
                             _));

  dual_model_->SetReadStatusIfExists(kUrl, false);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenSetCommonReadEntryRead) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now()).SetRead()},
      /*initial_account_entries_builders=*/
      {TestEntryBuilder(kUrl, clock_.Now())
           .SetRead(clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  dual_model_->SetReadStatusIfExists(kUrl, true);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenSyncMergeReadEntryWithUnreadEntryWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1)).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  scoped_refptr<const ReadingListEntry> merged_entry =
      local_or_syncable_model_ptr_->SyncMergeEntry(
          TestEntryBuilder(kUrl, clock_.Now()).Build());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenSyncMergeUnreadEntryWithReadEntryWhenSyncEnabled) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();
  scoped_refptr<const ReadingListEntry> merged_entry =
      local_or_syncable_model_ptr_->SyncMergeEntry(
          TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1))
              .SetRead()
              .Build());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenSyncMergeReadEntryWithUnreadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1)).SetRead()}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                              /*unread_size=*/0ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  scoped_refptr<const ReadingListEntry> merged_entry =
      account_model_ptr_->SyncMergeEntry(
          TestEntryBuilder(kUrl, clock_.Now()).Build());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/0ul,
                                            /*unread_size=*/0ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainCountsWhenSyncMergeUnreadEntryWithReadEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now() + base::Seconds(1))}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));

  EXPECT_CALL(observer_, ReadingListWillMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));
  EXPECT_CALL(observer_, ReadingListDidMoveEntry(
                             HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                              /*unread_size=*/1ul),
                             _));

  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();
  scoped_refptr<const ReadingListEntry> merged_entry =
      account_model_ptr_->SyncMergeEntry(
          TestEntryBuilder(kUrl, clock_.Now()).SetRead().Build());

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/1ul, /*unseen_size=*/1ul,
                                            /*unread_size=*/1ul));
}

TEST_F(DualReadingListModelTest,
       ShouldMaintainCountsWhenMarkAllForUploadToSyncServerIfNeeded) {
  const GURL kLocalUrl("http://local_url.com/");
  const GURL kAccountUrl("http://account_url.com/");
  const GURL kCommonUrl("http://common_url.com/");

  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kLocalUrl, clock_.Now()),
       TestEntryBuilder(kCommonUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountUrl, clock_.Now()),
          TestEntryBuilder(kCommonUrl, clock_.Now())}));
  ASSERT_TRUE(dual_model_->loaded());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonUrl),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/3ul, /*unseen_size=*/3ul,
                                            /*unread_size=*/3ul));

  dual_model_->MarkAllForUploadToSyncServerIfNeeded();

  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/3ul, /*unseen_size=*/3ul,
                                            /*unread_size=*/3ul));
}

TEST_F(
    DualReadingListModelTest,
    ShouldMaintainSeenAndReadStatusWhenMarkAllForUploadToSyncServerIfNeeded) {
  const GURL kUnseenLocalUrl("http://unseen_local_url.com/");
  const GURL kUnreadLocalUrl("http://unread_local_url.com/");
  const GURL kReadLocalUrl("http://read_local_url.com/");
  const GURL kUnseenAccountUrl("http://unseen_account_url.com/");
  const GURL kUnreadAccountUrl("http://unread_account_url.com/");
  const GURL kReadAccountUrl("http://read_account_url.com/");
  // Seen in account model but unseen in local model.
  const GURL kUnreadCommonUrl1("http://unread_common_url_1.com/");
  // Seen in local model but unseen in account model.
  const GURL kUnreadCommonUrl2("http://unread_common_url_2.com/");
  // Read in account model but unread in local model.
  const GURL kReadCommonUrl1("http://read_common_url_1.com/");
  // Read in local model but unread in account model.
  const GURL kReadCommonUrl2("http://read_common_url_2.com/");

  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_or_syncable_entries_builders=*/
      {TestEntryBuilder(kUnseenLocalUrl, clock_.Now()),
       TestEntryBuilder(kUnreadLocalUrl, clock_.Now()).SetRead(false),
       TestEntryBuilder(kReadLocalUrl, clock_.Now()).SetRead(),
       TestEntryBuilder(kUnreadCommonUrl1, clock_.Now()),
       TestEntryBuilder(kUnreadCommonUrl2, clock_.Now()).SetRead(false),
       TestEntryBuilder(kReadCommonUrl1, clock_.Now()),
       TestEntryBuilder(kReadCommonUrl2, clock_.Now())
           .SetRead(clock_.Now() + base::Seconds(1))},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUnseenAccountUrl, clock_.Now()),
          TestEntryBuilder(kUnreadAccountUrl, clock_.Now()).SetRead(false),
          TestEntryBuilder(kReadAccountUrl, clock_.Now()).SetRead(),
          TestEntryBuilder(kUnreadCommonUrl1, clock_.Now()).SetRead(false),
          TestEntryBuilder(kUnreadCommonUrl2, clock_.Now()),
          TestEntryBuilder(kReadCommonUrl1, clock_.Now())
              .SetRead(clock_.Now() + base::Seconds(1)),
          TestEntryBuilder(kReadCommonUrl2, clock_.Now())}));
  ASSERT_TRUE(dual_model_->loaded());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadCommonUrl1),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadCommonUrl2),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadCommonUrl1),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadCommonUrl2),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_THAT(dual_model_, HasCountersEqual(/*size=*/10ul, /*unseen_size=*/2ul,
                                            /*unread_size=*/6ul));

  dual_model_->MarkAllForUploadToSyncServerIfNeeded();

  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenLocalUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadLocalUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadLocalUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnseenAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadCommonUrl1),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kUnreadCommonUrl2),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadCommonUrl1),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kReadCommonUrl2),
            StorageStateForTesting::kExistsInAccountModelOnly);

  EXPECT_THAT(dual_model_, HasCountersEqual(/*size=*/10ul, /*unseen_size=*/2ul,
                                            /*unread_size=*/6ul));
}

TEST_F(DualReadingListModelTest,
       ShouldClearLocalModelUponMarkAllForUploadToSyncServerIfNeeded) {
  const GURL kLocalURL("http://local_url.com/");
  const GURL kAccountURL("http://account_url.com/");
  const GURL kCommonURL("http://common_url.com/");

  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kLocalURL,
                                                           clock_.Now())
                                              .SetTitle("local_entry_title"),
                                          TestEntryBuilder(kCommonURL,
                                                           clock_.Now())
                                              .SetTitle("common_entry_title")},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountURL, clock_.Now())
              .SetTitle("account_entry_title"),
          TestEntryBuilder(kCommonURL, clock_.Now())
              .SetTitle("common_entry_title")}));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalURL),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountURL),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonURL),
            StorageStateForTesting::kExistsInBothModels);

  ASSERT_TRUE(dual_model_->NeedsExplicitUploadToSyncServer(kLocalURL));
  ASSERT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kAccountURL));
  ASSERT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kCommonURL));

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListWillAddEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->MarkAllForUploadToSyncServerIfNeeded();

  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalURL),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountURL),
            StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonURL),
            StorageStateForTesting::kExistsInAccountModelOnly);

  EXPECT_THAT(dual_model_->GetEntryByURL(kLocalURL),
              MatchesEntry(kLocalURL, "local_entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kAccountURL),
              MatchesEntry(kAccountURL, "account_entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kCommonURL),
              MatchesEntry(kCommonURL, "common_entry_title"));

  // Local model should be cleared even though the common entry was not actually
  // moved.
  EXPECT_EQ(local_or_syncable_model_ptr_->size(), 0u);
}

TEST_F(DualReadingListModelTest,
       ShouldReturnAllLocalKeysUponGetKeysThatNeedUploadToSyncServer) {
  const GURL kLocalURL("http://local_url.com/");
  const GURL kAccountURL("http://account_url.com/");
  const GURL kCommonURL("http://common_url.com/");

  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kLocalURL,
                                                           clock_.Now())
                                              .SetTitle("local_entry_title"),
                                          TestEntryBuilder(kCommonURL,
                                                           clock_.Now())
                                              .SetTitle("common_entry_title")},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountURL, clock_.Now())
              .SetTitle("account_entry_title"),
          TestEntryBuilder(kCommonURL, clock_.Now())
              .SetTitle("common_entry_title")}));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalURL),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountURL),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonURL),
            StorageStateForTesting::kExistsInBothModels);

  base::flat_set<GURL> keys = dual_model_->GetKeysThatNeedUploadToSyncServer();
  EXPECT_EQ(keys, base::flat_set<GURL>({kLocalURL, kCommonURL}));
}

TEST_F(
    DualReadingListModelTest,
    ShouldReturnNullIfLocalOrSyncableModelIsSyncingUponGetKeysThatNeedUploadToSyncServer) {
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(/*initial_syncable_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  base::flat_set<GURL> keys = dual_model_->GetKeysThatNeedUploadToSyncServer();
  EXPECT_THAT(keys, ::testing::IsEmpty());
}

TEST_F(DualReadingListModelTest,
       GetAccountModelIfSyncingShouldNotReturnNullWhenSignedInSyncDisabled) {
  ResetStorageAndMimicSignedInSyncDisabled();
  ASSERT_THAT(dual_model_->GetAccountModelIfSyncing(), NotNull());
}

TEST_F(DualReadingListModelTest,
       GetAccountModelIfSyncingShouldReturnNullWhenSignedOut) {
  ResetStorageAndMimicSignedOut();
  ASSERT_THAT(dual_model_->GetAccountModelIfSyncing(), IsNull());
}

}  // namespace
}  // namespace reading_list
