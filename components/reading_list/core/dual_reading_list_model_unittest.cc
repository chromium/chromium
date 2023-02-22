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
      std::vector<scoped_refptr<ReadingListEntry>>
          initial_local_or_syncable_entries = {},
      std::vector<scoped_refptr<ReadingListEntry>> initial_account_entries =
          {}) {
    ResetStorage();
    return local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_local_or_syncable_entries)) &&
           account_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_account_entries));
  }

  bool ResetStorageAndMimicSignedOut(
      std::vector<scoped_refptr<ReadingListEntry>> initial_local_entries = {}) {
    return ResetStorageAndTriggerLoadCompletion(
        std::move(initial_local_entries), /*initial_account_entries=*/{});
  }

  bool ResetStorageAndMimicSignedInSyncDisabled(
      std::vector<scoped_refptr<ReadingListEntry>> initial_local_entries = {},
      std::vector<scoped_refptr<ReadingListEntry>> initial_account_entries =
          {}) {
    ResetStorage();
    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    metadata_batch->SetModelTypeState(state);
    return local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_local_entries)) &&
           account_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_account_entries), std::move(metadata_batch));
  }

  bool ResetStorageAndMimicSyncEnabled(
      std::vector<scoped_refptr<ReadingListEntry>> initial_syncable_entries =
          {}) {
    ResetStorage();
    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    metadata_batch->SetModelTypeState(state);
    return local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
               std::move(initial_syncable_entries),
               std::move(metadata_batch)) &&
           account_model_storage_ptr_->TriggerLoadCompletion();
  }

  std::vector<scoped_refptr<ReadingListEntry>> MakeTestEntriesForURLs(
      const std::vector<GURL>& urls) {
    std::vector<scoped_refptr<ReadingListEntry>> entries;
    for (const auto& url : urls) {
      entries.push_back(base::MakeRefCounted<ReadingListEntry>(
          url, "Title for " + url.spec(), clock_.Now()));
    }
    return entries;
  }

 protected:
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
  EXPECT_CALL(observer_, ReadingListModelLoaded(_)).Times(0);
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
  EXPECT_CALL(observer_, ReadingListModelLoaded(_)).Times(0);
  ResetStorage();
  ASSERT_TRUE(local_or_syncable_model_storage_ptr_->TriggerLoadCompletion(
      base::unexpected("Fake error")));
  ASSERT_TRUE(account_model_storage_ptr_->TriggerLoadCompletion());
  EXPECT_FALSE(dual_model_->loaded());
}

TEST_F(DualReadingListModelTest, ReturnAccountModelSize) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries=*/{},
      MakeTestEntriesForURLs({GURL("https://url.com")})));
  ASSERT_EQ(0ul, local_or_syncable_model_ptr_->size());
  ASSERT_EQ(1ul, account_model_ptr_->size());
  EXPECT_EQ(1ul, dual_model_->size());
}

TEST_F(DualReadingListModelTest, ReturnLocalModelSize) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      MakeTestEntriesForURLs({GURL("https://url.com")}),
      /*initial_account_entries=*/{}));
  ASSERT_EQ(1ul, local_or_syncable_model_ptr_->size());
  ASSERT_EQ(0ul, account_model_ptr_->size());
  EXPECT_EQ(1ul, dual_model_->size());
}

TEST_F(DualReadingListModelTest, ReturnKeysSize) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      MakeTestEntriesForURLs({GURL("https://url1.com")}),
      MakeTestEntriesForURLs({GURL("https://url2.com")})));
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
  EXPECT_CALL(observer_, ReadingListModelCompletedBatchUpdates(_)).Times(0);

  EXPECT_FALSE(dual_model_->IsPerformingBatchUpdates());

  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(dual_model_.get()));
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> batch =
      dual_model_->BeginBatchUpdates();
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // When two updates happen at the same time, the notification is only sent
  // for beginning of first update and completion of last update.
  EXPECT_CALL(observer_, ReadingListModelBeganBatchUpdates(_)).Times(0);

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
  std::vector<scoped_refptr<ReadingListEntry>> local_entries;
  local_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://local_url.com/"), "local_entry", clock_.Now()));

  auto local_common_entry1 = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://common_url1.com/"), "merged_entry_title_from_local_entry",
      clock_.Now() + base::Seconds(1));
  local_common_entry1->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  local_entries.push_back(local_common_entry1);

  auto local_common_entry2 = base::MakeRefCounted<ReadingListEntry>(
      GURL("http://common_url2.com/"), "merged_entry_title_from_local_entry",
      clock_.Now());
  local_common_entry2->SetDistilledState(ReadingListEntry::DISTILLATION_ERROR);
  local_entries.push_back(local_common_entry2);

  std::vector<scoped_refptr<ReadingListEntry>> account_entries;
  account_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://account_url.com/"), "account_entry", clock_.Now()));
  account_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://common_url1.com/"), "merged_entry_title_from_account_entry",
      clock_.Now()));
  account_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL("http://common_url2.com/"), "merged_entry_title_from_account_entry",
      clock_.Now() + base::Seconds(1)));

  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(std::move(local_entries),
                                                   std::move(account_entries)));
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
  const GURL kLocalUrl("http://local_url.com/");
  ASSERT_TRUE(
      ResetStorageAndMimicSignedOut(MakeTestEntriesForURLs({kLocalUrl})));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kLocalUrl));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(
      GURL("http://non_existing_url.com/")));
}

TEST_F(DualReadingListModelTest,
       NeedsExplicitUploadToSyncServerWhenSignedInSyncDisabled) {
  const GURL kLocalUrl("http://local_url.com/");
  const GURL kAccountUrl("http://account_url.com/");
  const GURL kCommonUrl("http://common_url.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      MakeTestEntriesForURLs({kLocalUrl, kCommonUrl}),
      MakeTestEntriesForURLs({kAccountUrl, kCommonUrl})));
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
  const GURL kSyncableUrl("http://syncable_url.com/");
  ASSERT_TRUE(
      ResetStorageAndMimicSyncEnabled(MakeTestEntriesForURLs({kSyncableUrl})));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kSyncableUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(kSyncableUrl));
  EXPECT_FALSE(dual_model_->NeedsExplicitUploadToSyncServer(
      GURL("http://non_existing_url.com/")));
}

TEST_F(DualReadingListModelTest, RemoveNonExistingEntryByUrl) {
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion());
  const GURL kNonExistingURL("http://non_existing_url.com/");

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kNonExistingURL),
            StorageStateForTesting::kNotFound);
  ASSERT_THAT(dual_model_->GetEntryByURL(kNonExistingURL), IsNull());

  EXPECT_CALL(observer_, ReadingListWillRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry).Times(0);
  EXPECT_CALL(observer_, ReadingListDidApplyChanges).Times(0);

  dual_model_->RemoveEntryByURL(kNonExistingURL);

  EXPECT_THAT(dual_model_->GetEntryByURL(kNonExistingURL), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveLocalEntryByUrl) {
  const GURL kLocalUrl("http://local_url.com/");
  ASSERT_TRUE(
      ResetStorageAndTriggerLoadCompletion(MakeTestEntriesForURLs({kLocalUrl}),
                                           /*initial_account_entries=*/{}));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kLocalUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillRemoveEntry(dual_model_.get(), kLocalUrl));
  EXPECT_CALL(observer_,
              ReadingListDidRemoveEntry(dual_model_.get(), kLocalUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->RemoveEntryByURL(kLocalUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kLocalUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveAccountEntryByUrl) {
  const GURL kAccountUrl("http://account_url.com/");
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries=*/{},
      MakeTestEntriesForURLs({kAccountUrl})));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kAccountUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillRemoveEntry(dual_model_.get(), kAccountUrl));
  EXPECT_CALL(observer_,
              ReadingListDidRemoveEntry(dual_model_.get(), kAccountUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->RemoveEntryByURL(kAccountUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kAccountUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveCommonEntryByUrl) {
  const GURL kCommonUrl("http://common_url.com/");
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      MakeTestEntriesForURLs({kCommonUrl}),
      MakeTestEntriesForURLs({kCommonUrl})));

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_THAT(dual_model_->GetEntryByURL(kCommonUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillRemoveEntry(dual_model_.get(), kCommonUrl));
  EXPECT_CALL(observer_,
              ReadingListDidRemoveEntry(dual_model_.get(), kCommonUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  dual_model_->RemoveEntryByURL(kCommonUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kCommonUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveLocalEntryByUrlFromSync) {
  const GURL kLocalUrl("http://local_url.com/");
  ASSERT_TRUE(
      ResetStorageAndTriggerLoadCompletion(MakeTestEntriesForURLs({kLocalUrl}),
                                           /*initial_account_entries=*/{}));
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = local_or_syncable_model_ptr_->BeginBatchUpdates();

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kLocalUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillRemoveEntry(dual_model_.get(), kLocalUrl));
  EXPECT_CALL(observer_,
              ReadingListDidRemoveEntry(dual_model_.get(), kLocalUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  local_or_syncable_model_ptr_->SyncRemoveEntry(kLocalUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kLocalUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveAccountEntryByUrlFromSync) {
  const GURL kAccountUrl("http://account_url.com/");
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      /*initial_local_or_syncable_entries=*/{},
      MakeTestEntriesForURLs({kAccountUrl})));
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kAccountUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);
  ASSERT_THAT(dual_model_->GetEntryByURL(kAccountUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillRemoveEntry(dual_model_.get(), kAccountUrl));
  EXPECT_CALL(observer_,
              ReadingListDidRemoveEntry(dual_model_.get(), kAccountUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  account_model_ptr_->SyncRemoveEntry(kAccountUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kAccountUrl), IsNull());
}

TEST_F(DualReadingListModelTest, RemoveCommonEntryByUrlFromSync) {
  const GURL kCommonUrl("http://common_url.com/");
  ASSERT_TRUE(ResetStorageAndTriggerLoadCompletion(
      MakeTestEntriesForURLs({kCommonUrl}),
      MakeTestEntriesForURLs({kCommonUrl})));
  // DCHECKs verify that sync updates are issued as batch updates.
  auto token = account_model_ptr_->BeginBatchUpdates();

  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kCommonUrl),
            StorageStateForTesting::kExistsInBothModels);
  ASSERT_THAT(dual_model_->GetEntryByURL(kCommonUrl), NotNull());

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillRemoveEntry(dual_model_.get(), kCommonUrl));
  EXPECT_CALL(observer_,
              ReadingListDidRemoveEntry(dual_model_.get(), kCommonUrl));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  account_model_ptr_->SyncRemoveEntry(kCommonUrl);

  EXPECT_THAT(dual_model_->GetEntryByURL(kCommonUrl), NotNull());
  EXPECT_THAT(account_model_ptr_->GetEntryByURL(kCommonUrl), IsNull());
}

TEST_F(DualReadingListModelTest, AddEntryWhenSignedOut) {
  ASSERT_TRUE(ResetStorageAndMimicSignedOut());
  const GURL kLocalUrl("http://local_url.com/");

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(_, _)).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kLocalUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kLocalUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kLocalUrl, "local_entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kLocalUrl, "local_entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kLocalUrl),
              MatchesEntry(kLocalUrl, "local_entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(kLocalUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest, AddEntryWhenSignedInSyncDisabled) {
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled());
  const GURL kAccountUrl("http://account_url.com/");

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(_, _)).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kAccountUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kAccountUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kAccountUrl, "account_entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kAccountUrl, "account_entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kAccountUrl),
              MatchesEntry(kAccountUrl, "account_entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(entry->URL()),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest, AddEntryWhenSyncEnabled) {
  ASSERT_TRUE(ResetStorageAndMimicSyncEnabled());
  const GURL kSyncableUrl("http://syncable_url.com/");

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListWillRemoveEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidRemoveEntry(_, _)).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(observer_,
              ReadingListWillAddEntry(dual_model_.get(), HasUrl(kSyncableUrl)));
  EXPECT_CALL(observer_,
              ReadingListDidAddEntry(dual_model_.get(), kSyncableUrl,
                                     reading_list::ADDED_VIA_CURRENT_APP));
  EXPECT_CALL(observer_, ReadingListDidApplyChanges(dual_model_.get()));

  scoped_refptr<const ReadingListEntry> entry = &dual_model_->AddOrReplaceEntry(
      kSyncableUrl, "syncable_entry_title", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  EXPECT_THAT(entry, MatchesEntry(kSyncableUrl, "syncable_entry_title"));
  EXPECT_THAT(dual_model_->GetEntryByURL(kSyncableUrl),
              MatchesEntry(kSyncableUrl, "syncable_entry_title"));
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(entry->URL()),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);
}

TEST_F(DualReadingListModelTest,
       AddLocalExistingEntryWhenSignedInSyncDisabled) {
  const GURL kUrl("http://url.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      MakeTestEntriesForURLs({kUrl}), /*initial_account_entries=*/{}));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInLocalOrSyncableModelOnly);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);

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
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(entry->URL()),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest,
       AddAccountExistingEntryWhenSignedInSyncDisabled) {
  const GURL kUrl("http://url.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      /*initial_local_entries=*/{}, MakeTestEntriesForURLs({kUrl})));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInAccountModelOnly);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);

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
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(entry->URL()),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(DualReadingListModelTest,
       AddCommonExistingEntryWhenSignedInSyncDisabled) {
  const GURL kUrl("http://url.com/");
  ASSERT_TRUE(ResetStorageAndMimicSignedInSyncDisabled(
      MakeTestEntriesForURLs({kUrl}), MakeTestEntriesForURLs({kUrl})));
  ASSERT_EQ(dual_model_->GetStorageStateForURLForTesting(kUrl),
            StorageStateForTesting::kExistsInBothModels);

  EXPECT_CALL(observer_, ReadingListWillUpdateEntry(_, _)).Times(0);
  EXPECT_CALL(observer_, ReadingListDidUpdateEntry(_, _)).Times(0);

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
  EXPECT_EQ(dual_model_->GetStorageStateForURLForTesting(entry->URL()),
            StorageStateForTesting::kExistsInAccountModelOnly);
}

}  // namespace
}  // namespace reading_list
