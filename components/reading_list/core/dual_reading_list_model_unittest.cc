// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/dual_reading_list_model.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/storage_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace reading_list {
namespace {

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

  TestEntryBuilder& SetRead() {
    update_read_time_ = creation_time_;
    return *this;
  }

  TestEntryBuilder& SetRead(base::Time now) {
    update_read_time_ = now;
    return *this;
  }

  TestEntryBuilder& SetDistilledState(
      ReadingListEntry::DistillationState distilation_state) {
    distilation_state_ = distilation_state;
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
      entry->SetRead(true, update_read_time_.value());
    }

    if (distilation_state_.has_value()) {
      entry->SetDistilledState(distilation_state_.value());
    }

    return entry;
  }

 private:
  const GURL url_;
  const base::Time creation_time_;

  absl::optional<std::pair<std::string, base::Time>> title_and_update_time_;
  absl::optional<base::Time> update_read_time_;
  absl::optional<ReadingListEntry::DistillationState> distilation_state_;
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
        syncer::StorageType::kUnspecified, &clock_);
    local_or_syncable_model_ptr_ = local_or_syncable_model.get();

    auto account_model_storage =
        std::make_unique<FakeReadingListModelStorage>();
    account_model_storage_ptr_ = account_model_storage->AsWeakPtr();
    auto account_model = std::make_unique<ReadingListModelImpl>(
        std::move(account_model_storage), syncer::StorageType::kAccount,
        &clock_);
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

  bool ResetStorageAndMimicSignedInSyncDisabled(
      std::vector<TestEntryBuilder> initial_local_entries_builders = {},
      std::vector<TestEntryBuilder> initial_account_entries_builders = {}) {
    ResetStorage();

    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    metadata_batch->SetModelTypeState(state);

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

  bool ResetStorageAndMimicSyncEnabled(
      std::vector<TestEntryBuilder> initial_syncable_entries_builders = {}) {
    ResetStorage();

    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    metadata_batch->SetModelTypeState(state);

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
  base::SimpleTestClock clock_;
  testing::NiceMock<MockReadingListModelObserver> observer_;
  base::WeakPtr<FakeReadingListModelStorage>
      local_or_syncable_model_storage_ptr_;
  base::WeakPtr<FakeReadingListModelStorage> account_model_storage_ptr_;
  // Owned by `dual_model_` and guaranteed to exist while `dual_model_` exists.
  base::raw_ptr<ReadingListModelImpl> local_or_syncable_model_ptr_;
  base::raw_ptr<ReadingListModelImpl> account_model_ptr_;
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
  // updated, and initially the update time of the title is equal to the
  // creation time.
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
  const GURL kLocalUrl("http://local_url.com/");
  const GURL kAccountUrl("http://account_url.com/");
  const GURL kCommonUrl("http://common_url.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries_builders=*/{TestEntryBuilder(kLocalUrl,
                                                           clock_.Now()),
                                          TestEntryBuilder(kCommonUrl,
                                                           clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kAccountUrl, clock_.Now()),
          TestEntryBuilder(kCommonUrl, clock_.Now())}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonUrl),
            StorageStateForTesting::kExistsInBothModels);

  EXPECT_TRUE(dual_model_->NeedsExplicitUploadToSyncServer(kLocalUrl));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kAccountUrl));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kCommonUrl));
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

TEST_F(DualReadingListModelTest, RemoveNonExistingEntryByUrl) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->RemoveEntryByURL(kUrl);

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

  dual_model_->RemoveEntryByURL(kUrl);

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

  dual_model_->RemoveEntryByURL(kUrl);

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

  dual_model_->RemoveEntryByURL(kUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveLocalEntryByUrlFromSync) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{}));
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  local_or_syncable_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveAccountEntryByUrlFromSync) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveCommonEntryByUrlFromSync) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
          kUrl, clock_.Now())},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(dual_model_.get(), kUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  account_model_ptr_->SyncRemoveEntry(kUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kUrl), NotNull());
  EXPECT_THAT(account_model_ptr_->GetEntryByURL(kUrl), IsNull());
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
      /*initial_local_or_syncable_entries_builders=*/{TestEntryBuilder(
                                                          kUrl, clock_.Now())
                                                          .SetRead(
                                                              clock_.Now() +
                                                              base::Seconds(
                                                                  1))},
      /*initial_account_entries_builders=*/{
          TestEntryBuilder(kUrl, clock_.Now())}));
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
}

}  // namespace
}  // namespace reading_list
