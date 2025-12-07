// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_local_data_batch_uploader.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reading_list {
namespace {

using ::syncer::IsEmptyLocalDataDescription;
using ::syncer::MatchesLocalDataDescription;
using ::syncer::MatchesLocalDataItemModel;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;

class ReadingListLocalDataBatchUploaderTest : public ::testing::Test {
 public:
  ReadingListLocalDataBatchUploaderTest() {
    ON_CALL(processor_, IsTrackingMetadata).WillByDefault(Return(true));
    // `dual_reading_list_model_` takes ownership of the raw pointers below.
    local_reading_list_storage_ = new FakeReadingListModelStorage();
    account_reading_list_storage_ = new FakeReadingListModelStorage();
    dual_reading_list_model_ = std::make_unique<DualReadingListModel>(
        std::make_unique<ReadingListModelImpl>(
            std::unique_ptr<ReadingListModelStorage>(
                local_reading_list_storage_),
            syncer::StorageType::kUnspecified,
            syncer::WipeModelUponSyncDisabledBehavior::kNever, &clock_),
        ReadingListModelImpl::BuildNewForTest(
            std::unique_ptr<ReadingListModelStorage>(
                account_reading_list_storage_),
            syncer::StorageType::kAccount,
            syncer::WipeModelUponSyncDisabledBehavior::kAlways, &clock_,
            processor_.CreateForwardingProcessor()));
  }

  DualReadingListModel* dual_reading_list_model() {
    return dual_reading_list_model_.get();
  }

  void LoadModel() {
    local_reading_list_storage_->TriggerLoadCompletion();
    account_reading_list_storage_->TriggerLoadCompletion();
    ASSERT_TRUE(dual_reading_list_model_->loaded());
  }

 private:
  base::SimpleTestClock clock_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  std::unique_ptr<DualReadingListModel> dual_reading_list_model_;
  // The raw pointers must be destroyed before `dual_reading_list_model_`.
  raw_ptr<FakeReadingListModelStorage> local_reading_list_storage_;
  raw_ptr<FakeReadingListModelStorage> account_reading_list_storage_;
};

TEST_F(ReadingListLocalDataBatchUploaderTest, DescriptionEmptyIfModelNull) {
  ReadingListLocalDataBatchUploader uploader(nullptr);
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
}

TEST_F(ReadingListLocalDataBatchUploaderTest,
       DescriptionEmptyIfModelNotLoaded) {
  ReadingListLocalDataBatchUploader uploader(dual_reading_list_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
}

TEST_F(ReadingListLocalDataBatchUploaderTest, DescriptionHasOnlyLocalData) {
  LoadModel();
  dual_reading_list_model()->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      GURL("https://local.com"), "local", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  dual_reading_list_model()->GetAccountModelIfSyncing()->AddOrReplaceEntry(
      GURL("https://account.com"), "account",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  ReadingListLocalDataBatchUploader uploader(dual_reading_list_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(),
              MatchesLocalDataDescription(
                  syncer::DataType::READING_LIST,
                  ElementsAre(MatchesLocalDataItemModel(
                      GURL("https://local.com"),
                      syncer::LocalDataItemModel::PageUrlIcon(
                          GURL("https://local.com/")),
                      /*title=*/"local", /*subtitle=*/IsEmpty())),
                  /*item_count=*/1u, /*domains=*/ElementsAre("local.com"),
                  /*domain_count=*/1u));
}

TEST_F(ReadingListLocalDataBatchUploaderTest,
       LocalDescriptionEmptyItemsWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kSyncReadingListBatchUploadSelectedItems);

  LoadModel();
  dual_reading_list_model()->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      GURL("https://local.com"), "local", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  dual_reading_list_model()->GetAccountModelIfSyncing()->AddOrReplaceEntry(
      GURL("https://account.com"), "account",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  ReadingListLocalDataBatchUploader uploader(dual_reading_list_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(),
              MatchesLocalDataDescription(_, /*local_data_models=*/IsEmpty(),
                                          /*item_count=*/1u,
                                          /*domains=*/ElementsAre("local.com"),
                                          /*domain_count=*/1u));
}

TEST_F(ReadingListLocalDataBatchUploaderTest, MigrationNoOpsIfModelNull) {
  ReadingListLocalDataBatchUploader uploader(nullptr);

  uploader.TriggerLocalDataMigration();
  uploader.TriggerLocalDataMigrationForItems(
      {syncer::LocalDataItemModel::DataId(GURL("https://local.com"))});

  // Should not crash;
}

TEST_F(ReadingListLocalDataBatchUploaderTest, MigrationNoOpsIfModelNotLoaded) {
  ReadingListLocalDataBatchUploader uploader(dual_reading_list_model());

  uploader.TriggerLocalDataMigration();
  uploader.TriggerLocalDataMigrationForItems(
      {syncer::LocalDataItemModel::DataId(GURL("https://local.com"))});

  // Should not crash;
}

TEST_F(ReadingListLocalDataBatchUploaderTest, MigrationUploadsLocalData) {
  LoadModel();
  dual_reading_list_model()->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      GURL("https://local.com"), "local", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  dual_reading_list_model()->GetAccountModelIfSyncing()->AddOrReplaceEntry(
      GURL("https://account.com"), "account",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  ReadingListLocalDataBatchUploader uploader(dual_reading_list_model());

  uploader.TriggerLocalDataMigration();

  EXPECT_EQ(
      dual_reading_list_model()->GetStorageStateForURLForTesting(
          GURL("https://local.com")),
      DualReadingListModel::StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(
      dual_reading_list_model()->GetStorageStateForURLForTesting(
          GURL("https://account.com")),
      DualReadingListModel::StorageStateForTesting::kExistsInAccountModelOnly);
}

TEST_F(ReadingListLocalDataBatchUploaderTest, OnlySelectedItemsGetUploaded) {
  LoadModel();
  dual_reading_list_model()->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      GURL("https://local1.com"), "local1", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  dual_reading_list_model()->GetLocalOrSyncableModel()->AddOrReplaceEntry(
      GURL("https://local2.com"), "local2", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  dual_reading_list_model()->GetAccountModelIfSyncing()->AddOrReplaceEntry(
      GURL("https://account.com"), "account",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);
  ReadingListLocalDataBatchUploader uploader(dual_reading_list_model());

  ASSERT_EQ(dual_reading_list_model()->GetStorageStateForURLForTesting(
                GURL("https://local1.com")),
            DualReadingListModel::StorageStateForTesting::
                kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(dual_reading_list_model()->GetStorageStateForURLForTesting(
                GURL("https://local2.com")),
            DualReadingListModel::StorageStateForTesting::
                kExistsInLocalOrSyncableModelOnly);
  ASSERT_EQ(
      dual_reading_list_model()->GetStorageStateForURLForTesting(
          GURL("https://account.com")),
      DualReadingListModel::StorageStateForTesting::kExistsInAccountModelOnly);

  uploader.TriggerLocalDataMigrationForItems(
      {syncer::LocalDataItemModel::DataId(GURL("https://local2.com"))});

  EXPECT_EQ(dual_reading_list_model()->GetStorageStateForURLForTesting(
                GURL("https://local1.com")),
            DualReadingListModel::StorageStateForTesting::
                kExistsInLocalOrSyncableModelOnly);

  EXPECT_EQ(
      dual_reading_list_model()->GetStorageStateForURLForTesting(
          GURL("https://local2.com")),
      DualReadingListModel::StorageStateForTesting::kExistsInAccountModelOnly);
  EXPECT_EQ(
      dual_reading_list_model()->GetStorageStateForURLForTesting(
          GURL("https://account.com")),
      DualReadingListModel::StorageStateForTesting::kExistsInAccountModelOnly);
}

}  // namespace
}  // namespace reading_list
