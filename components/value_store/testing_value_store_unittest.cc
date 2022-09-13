// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/testing_value_store.h"
#include "components/value_store/value_store_test_suite.h"

namespace value_store {

class ValueStore;

namespace {

ValueStore* Param(const base::FilePath& file_path) {
  return new TestingValueStore();
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(TestingValueStore,
                         ValueStoreTestSuite,
                         testing::Values(&Param));

}  // namespace value_store
