// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_TEST_MODEL_TYPE_STORE_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_TEST_MODEL_TYPE_STORE_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"

namespace syncer {

class ModelTypeStoreBackend;

// Test-only ModelTypeStoreService implementation that uses a temporary dir
// for GetSyncDataPath() and uses in-memory storage for ModelTypeStore.
class TestModelTypeStoreService : public ModelTypeStoreService {
 public:
  TestModelTypeStoreService();
  ~TestModelTypeStoreService() override;

  // ModelTypeStoreService:
  const base::FilePath& GetSyncDataPath() const override;
  RepeatingModelTypeStoreFactory GetStoreFactory() override;
  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() override;

 private:
  const scoped_refptr<ModelTypeStoreBackend> store_backend_;
  base::ScopedTempDir sync_data_path_;

  DISALLOW_COPY_AND_ASSIGN(TestModelTypeStoreService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_TEST_MODEL_TYPE_STORE_SERVICE_H_
