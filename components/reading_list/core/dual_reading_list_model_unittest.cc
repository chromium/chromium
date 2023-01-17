// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/dual_reading_list_model.h"

#include "base/test/simple_test_clock.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/storage_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

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

    auto account_model_storage =
        std::make_unique<FakeReadingListModelStorage>();
    account_model_storage_ptr_ = account_model_storage->AsWeakPtr();
    auto account_model = std::make_unique<ReadingListModelImpl>(
        std::move(account_model_storage), syncer::StorageType::kUnspecified,
        &clock_);

    dual_model_ = std::make_unique<reading_list::DualReadingListModel>(
        std::move(local_or_syncable_model), std::move(account_model));
    dual_model_->AddObserver(&observer_);
  }

  size_t UnreadSize() {
    size_t size = 0;
    for (const auto& url : dual_model_->GetKeys()) {
      const ReadingListEntry* entry = dual_model_->GetEntryByURL(url);
      if (!entry->IsRead()) {
        size++;
      }
    }
    DCHECK_EQ(size, dual_model_->unread_size());
    return size;
  }

  size_t ReadSize() {
    size_t size = 0;
    for (const auto& url : dual_model_->GetKeys()) {
      const ReadingListEntry* entry = dual_model_->GetEntryByURL(url);
      if (entry->IsRead()) {
        size++;
      }
    }
    return size;
  }

 protected:
  base::SimpleTestClock clock_;
  testing::NiceMock<MockReadingListModelObserver> observer_;
  base::WeakPtr<FakeReadingListModelStorage>
      local_or_syncable_model_storage_ptr_;
  base::WeakPtr<FakeReadingListModelStorage> account_model_storage_ptr_;
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
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
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

}  // namespace
