// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_model_type_store_service.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/model/blocking_model_type_store_impl.h"
#include "components/sync/model/model_type_store_backend.h"
#include "components/sync/test/model_type_store_test_util.h"

namespace syncer {

TestModelTypeStoreService::TestModelTypeStoreService()
    : store_backend_(ModelTypeStoreBackend::CreateInMemoryForTest()) {
  DCHECK(sync_data_path_.CreateUniqueTempDir());
}

TestModelTypeStoreService::~TestModelTypeStoreService() = default;

const base::FilePath& TestModelTypeStoreService::GetSyncDataPath() const {
  return sync_data_path_.GetPath();
}

RepeatingModelTypeStoreFactory TestModelTypeStoreService::GetStoreFactory() {
  return ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
}

scoped_refptr<base::SequencedTaskRunner>
TestModelTypeStoreService::GetBackendTaskRunner() {
  return base::SequencedTaskRunnerHandle::Get();
}

}  // namespace syncer
