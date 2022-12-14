// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_MODEL_TYPE_STORE_SERVICE_H_
#define COMPONENTS_SYNC_TEST_TEST_MODEL_TYPE_STORE_SERVICE_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"

namespace syncer {

// Test-only ModelTypeStoreService implementation that uses a temporary dir
// for GetSyncDataPath() and uses in-memory storage for ModelTypeStore.
class TestModelTypeStoreService : public ModelTypeStoreService {
 public:
  TestModelTypeStoreService();

  TestModelTypeStoreService(const TestModelTypeStoreService&) = delete;
  TestModelTypeStoreService& operator=(const TestModelTypeStoreService&) =
      delete;

  ~TestModelTypeStoreService() override;

  // ModelTypeStoreService:
  const base::FilePath& GetSyncDataPath() const override;
  RepeatingModelTypeStoreFactory GetStoreFactory() override;
  RepeatingModelTypeStoreFactory GetStoreFactoryForAccountStorage() override;
  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() override;

 private:
  base::ScopedTempDir sync_data_path_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_MODEL_TYPE_STORE_SERVICE_H_
