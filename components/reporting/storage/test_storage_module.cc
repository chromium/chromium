// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/test_storage_module.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::WithArg;

namespace reporting {
namespace test {

TestStorageModuleStrict::TestStorageModuleStrict() {
  ON_CALL(*this, AddRecord)
      .WillByDefault(
          Invoke(this, &TestStorageModuleStrict::AddRecordSuccessfully));
  ON_CALL(*this, Flush)
      .WillByDefault(
          WithArg<1>(Invoke([](StorageModuleInterface::FlushCallback callback) {
            std::move(callback).Run(Status::StatusOK());
          })));
}

TestStorageModuleStrict::~TestStorageModuleStrict() = default;

const Record& TestStorageModuleStrict::record() const {
  EXPECT_TRUE(record_.has_value());
  return record_.value();
}

Priority TestStorageModuleStrict::priority() const {
  EXPECT_TRUE(priority_.has_value());
  return priority_.value();
}

void TestStorageModuleStrict::AddRecordSuccessfully(Priority priority,
                                                    Record record,
                                                    EnqueueCallback callback) {
  record_ = std::move(record);
  priority_ = priority;
  std::move(callback).Run(Status::StatusOK());
}

}  // namespace test
}  // namespace reporting
