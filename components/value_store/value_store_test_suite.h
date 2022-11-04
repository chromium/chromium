// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_TEST_SUITE_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_TEST_SUITE_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/value_store/value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace value_store {

// Parameter type for the value-parameterized tests.
typedef ValueStore* (*ValueStoreTestParam)(const base::FilePath& file_path);

// Test fixture for ValueStore tests. Tests are defined in
// value_store_test_suite.cc with configurations for both cached
// and non-cached leveldb storage, and cached no-op storage.
class ValueStoreTestSuite : public testing::TestWithParam<ValueStoreTestParam> {
 public:
  ValueStoreTestSuite();
  ~ValueStoreTestSuite() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<ValueStore> storage_;

 private:
  base::ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_TEST_SUITE_H_
