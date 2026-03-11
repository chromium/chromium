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
  base::test::TaskEnvironment task_environment_;
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
      "task1", get_future.GetCallback());

  std::vector<FilterAnnotation> result = get_future.Get();
  ASSERT_THAT(result, SizeIs(1));

  EXPECT_EQ(result.front(), annotation);
}

}  // namespace

}  // namespace multistep_filter
