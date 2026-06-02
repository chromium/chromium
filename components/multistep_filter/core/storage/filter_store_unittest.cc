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

using testing::Optional;
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

TEST_F(FilterStoreTest, DeleteAnnotationsForTask) {
  base::test::TestFuture<bool> store_future1;
  base::test::TestFuture<bool> store_future2;
  base::test::TestFuture<bool> store_future3;
  base::test::TestFuture<std::optional<int64_t>> expire_future;
  base::test::TestFuture<std::vector<FilterAnnotation>> get_future1;
  base::test::TestFuture<std::vector<FilterAnnotation>> get_future2;

  const base::Uuid id1 = base::Uuid::GenerateRandomV4();
  const FilterAnnotation annotation1(id1, "task1", "example1.com",
                                     base::Time::Now(), {});
  const base::Uuid id2 = base::Uuid::GenerateRandomV4();
  const FilterAnnotation annotation2(id2, "task1", "example2.com",
                                     base::Time::Now(), {});
  const base::Uuid id3 = base::Uuid::GenerateRandomV4();
  const FilterAnnotation annotation3(id3, "task2", "example3.com",
                                     base::Time::Now(), {});

  store()->StoreAnnotation(annotation1, store_future1.GetCallback());
  store()->StoreAnnotation(annotation2, store_future2.GetCallback());
  store()->StoreAnnotation(annotation3, store_future3.GetCallback());
  ASSERT_TRUE(store_future1.Get());
  ASSERT_TRUE(store_future2.Get());
  ASSERT_TRUE(store_future3.Get());

  store()->DeleteAnnotationsForTask("task1", expire_future.GetCallback());
  EXPECT_THAT(expire_future.Get(), Optional(2));

  store()->GetAnnotationsForTaskSortedByCreationTimestamp(
      "task1", get_future1.GetCallback(), kMaxResults, base::Time());
  EXPECT_THAT(get_future1.Get(), SizeIs(0));

  store()->GetAnnotationsForTaskSortedByCreationTimestamp(
      "task2", get_future2.GetCallback(), kMaxResults, base::Time());
  EXPECT_THAT(get_future2.Get(), SizeIs(1));
}

TEST_F(FilterStoreTest,
       GetAnnotationsForTaskSortedByCreationTimestamp_ExcludesDeleted) {
  base::test::TestFuture<bool> store_future;
  base::test::TestFuture<std::optional<int64_t>> expire_future;
  base::test::TestFuture<std::vector<FilterAnnotation>> get_future;

  const base::Uuid id = base::Uuid::GenerateRandomV4();
  const FilterAnnotation annotation(id, "task1", "example.com",
                                    base::Time::Now(), {});

  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  store()->DeleteAnnotationsForTask("task1", expire_future.GetCallback());
  EXPECT_THAT(expire_future.Get(), Optional(1));

  store()->GetAnnotationsForTaskSortedByCreationTimestamp(
      "task1", get_future.GetCallback(), kMaxResults, base::Time());
  EXPECT_THAT(get_future.Get(), SizeIs(0));
}

TEST_F(FilterStoreTest, DeleteAnnotationsForDomains) {
  base::test::TestFuture<bool> store_future1;
  base::test::TestFuture<bool> store_future2;
  base::test::TestFuture<bool> store_future3;
  base::test::TestFuture<std::optional<int64_t>> delete_future;
  base::test::TestFuture<std::vector<FilterAnnotation>> get_future1;
  base::test::TestFuture<std::vector<FilterAnnotation>> get_future2;

  base::Time now = base::Time::Now();
  const base::Uuid id1 = base::Uuid::GenerateRandomV4();
  const FilterAnnotation annotation1(id1, "task1", "example1.com", now, {});
  const base::Uuid id2 = base::Uuid::GenerateRandomV4();
  const FilterAnnotation annotation2(id2, "task1", "example2.com", now, {});
  const base::Uuid id3 = base::Uuid::GenerateRandomV4();
  const FilterAnnotation annotation3(id3, "task2", "example1.com",
                                     now - base::Hours(2), {});

  store()->StoreAnnotation(annotation1, store_future1.GetCallback());
  store()->StoreAnnotation(annotation2, store_future2.GetCallback());
  store()->StoreAnnotation(annotation3, store_future3.GetCallback());
  ASSERT_TRUE(store_future1.Get());
  ASSERT_TRUE(store_future2.Get());
  ASSERT_TRUE(store_future3.Get());

  // Delete data for example1.com in the last hour.
  // It should only delete annotation1!
  store()->DeleteAnnotationsForDomains({"example1.com"}, now - base::Hours(1),
                                       now + base::Hours(1),
                                       delete_future.GetCallback());
  EXPECT_THAT(delete_future.Get(), Optional(1));

  // task1 should only have annotation2 remaining.
  store()->GetAnnotationsForTaskSortedByCreationTimestamp(
      "task1", get_future1.GetCallback(), kMaxResults, base::Time());
  std::vector<FilterAnnotation> result1 = get_future1.Get();
  ASSERT_THAT(result1, SizeIs(1));
  EXPECT_EQ(result1[0].id, id2);

  // task2 should still have annotation3 (since it was out of time range).
  store()->GetAnnotationsForTaskSortedByCreationTimestamp(
      "task2", get_future2.GetCallback(), kMaxResults, base::Time());
  std::vector<FilterAnnotation> result2 = get_future2.Get();
  ASSERT_THAT(result2, SizeIs(1));
  EXPECT_EQ(result2[0].id, id3);
}

}  // namespace

}  // namespace multistep_filter
