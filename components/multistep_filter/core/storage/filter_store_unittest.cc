// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_store.h"

#include <memory>

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/multistep_filter/core/storage/filter_store_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

class FilterStoreTest : public testing::Test {
 public:
  FilterStoreTest() = default;
  ~FilterStoreTest() override = default;

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

}  // namespace multistep_filter
