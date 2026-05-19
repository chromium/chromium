// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_store.h"

#include <memory>
#include <vector>

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/storage/filter_store_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

namespace {

using testing::SizeIs;

constexpr size_t kMaxResults = 100;

class FilterStoreTest : public testing::Test {
 public:
  void SetUp() override { store_ = std::make_unique<FilterStore>(); }

  void TearDown() override {
    if (store_) {
      store_.reset();
      base::ThreadPoolInstance::Get()->FlushForTesting();
    }
  }

  FilterStore* store() { return store_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FilterStore> store_;
};

TEST_F(FilterStoreTest, StoreAndRetrieveAnnotation) {
  base::test::TestFuture<bool> store_future;
  base::test::TestFuture<std::vector<FilterAnnotation>> get_future;

  base::Uuid id = base::Uuid::GenerateRandomV4();
  std::vector<FilterAttribute> attributes;
  attributes.emplace_back("key1", "value1");
  FilterAnnotation annotation(id, "task1", "example.com", base::Time::Now(),
                              attributes);

  store()->StoreAnnotation(annotation, store_future.GetCallback());
  store()->GetAnnotationsForTaskSortedByCreationTimestamp(
      "task1", get_future.GetCallback(), kMaxResults, base::Time());

  std::vector<FilterAnnotation> result = get_future.Get();
  ASSERT_THAT(result, SizeIs(1));

  EXPECT_EQ(result.front(), annotation);
}

TEST_F(FilterStoreTest, StoreAndRetrieveAnnotation_FiltersByCreationTime) {
  base::test::TestFuture<bool> store_future1;
  base::test::TestFuture<bool> store_future2;
  base::test::TestFuture<std::vector<FilterAnnotation>> get_future;

  base::Uuid id1 = base::Uuid::GenerateRandomV4();
  FilterAnnotation old_annotation(id1, "task1", "example.com",
                                  base::Time::Now() - base::Minutes(31), {});

  base::Uuid id2 = base::Uuid::GenerateRandomV4();
  FilterAnnotation recent_annotation(id2, "task1", "example.com",
                                     base::Time::Now(), {});

  store()->StoreAnnotation(old_annotation, store_future1.GetCallback());
  store()->StoreAnnotation(recent_annotation, store_future2.GetCallback());
  ASSERT_TRUE(store_future1.Get());
  ASSERT_TRUE(store_future2.Get());

  store()->GetAnnotationsForTaskSortedByCreationTimestamp(
      "task1", get_future.GetCallback(), kMaxResults,
      base::Time::Now() - base::Minutes(30));

  std::vector<FilterAnnotation> result = get_future.Get();
  ASSERT_THAT(result, SizeIs(1));
  EXPECT_EQ(result.front(), recent_annotation);
}

}  // namespace

}  // namespace multistep_filter
