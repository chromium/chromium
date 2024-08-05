// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_data_type_store_service.h"

#include "base/task/sequenced_task_runner.h"
#include "components/sync/test/data_type_store_test_util.h"

namespace syncer {

TestDataTypeStoreService::TestDataTypeStoreService() {
  DCHECK(sync_data_path_.CreateUniqueTempDir());
}

TestDataTypeStoreService::~TestDataTypeStoreService() = default;

const base::FilePath& TestDataTypeStoreService::GetSyncDataPath() const {
  return sync_data_path_.GetPath();
}

RepeatingDataTypeStoreFactory TestDataTypeStoreService::GetStoreFactory() {
  return DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
}

RepeatingDataTypeStoreFactory
TestDataTypeStoreService::GetStoreFactoryForAccountStorage() {
  return DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
}

scoped_refptr<base::SequencedTaskRunner>
TestDataTypeStoreService::GetBackendTaskRunner() {
  return base::SequencedTaskRunner::GetCurrentDefault();
}

}  // namespace syncer
