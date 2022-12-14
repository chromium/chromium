// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_model_type_store_service.h"

#include "base/task/sequenced_task_runner.h"
#include "components/sync/test/model_type_store_test_util.h"

namespace syncer {

TestModelTypeStoreService::TestModelTypeStoreService() {
  DCHECK(sync_data_path_.CreateUniqueTempDir());
}

TestModelTypeStoreService::~TestModelTypeStoreService() = default;

const base::FilePath& TestModelTypeStoreService::GetSyncDataPath() const {
  return sync_data_path_.GetPath();
}

RepeatingModelTypeStoreFactory TestModelTypeStoreService::GetStoreFactory() {
  return ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
}

RepeatingModelTypeStoreFactory
TestModelTypeStoreService::GetStoreFactoryForAccountStorage() {
  return ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
}

scoped_refptr<base::SequencedTaskRunner>
TestModelTypeStoreService::GetBackendTaskRunner() {
  return base::SequencedTaskRunner::GetCurrentDefault();
}

}  // namespace syncer
