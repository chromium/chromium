// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_TEST_STORAGE_MODULE_H_
#define COMPONENTS_REPORTING_STORAGE_TEST_STORAGE_MODULE_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

class TestStorageModuleStrict : public StorageModuleInterface {
 public:
  // As opposed to the production |StorageModule|, test module does not need to
  // call factory method - it is created directly by constructor.
  TestStorageModuleStrict();

  MOCK_METHOD(void,
              AddRecord,
              (Priority priority, Record record, EnqueueCallback callback),
              (override));

  MOCK_METHOD(void,
              Flush,
              (Priority priority, FlushCallback callback),
              (override));

  const Record& record() const;
  Priority priority() const;

 protected:
  ~TestStorageModuleStrict() override;

 private:
  void AddRecordSuccessfully(Priority priority,
                             Record record,
                             EnqueueCallback callback);

  std::optional<Record> record_;
  std::optional<Priority> priority_;
};

// Most of the time no need to log uninterested calls to |AddRecord|.
typedef ::testing::NiceMock<TestStorageModuleStrict> TestStorageModule;

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_TEST_STORAGE_MODULE_H_
