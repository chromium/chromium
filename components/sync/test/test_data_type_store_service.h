// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_DATA_TYPE_STORE_SERVICE_H_
#define COMPONENTS_SYNC_TEST_TEST_DATA_TYPE_STORE_SERVICE_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"

namespace syncer {

// Test-only DataTypeStoreService implementation that uses a temporary dir
// for GetSyncDataPath() and uses in-memory storage for DataTypeStore.
class TestDataTypeStoreService : public DataTypeStoreService {
 public:
  TestDataTypeStoreService();

  TestDataTypeStoreService(const TestDataTypeStoreService&) = delete;
  TestDataTypeStoreService& operator=(const TestDataTypeStoreService&) = delete;

  ~TestDataTypeStoreService() override;

  // DataTypeStoreService:
  const base::FilePath& GetSyncDataPath() const override;
  RepeatingDataTypeStoreFactory GetStoreFactory() override;
  RepeatingDataTypeStoreFactory GetStoreFactoryForAccountStorage() override;
  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() override;

 private:
  base::ScopedTempDir sync_data_path_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_DATA_TYPE_STORE_SERVICE_H_
