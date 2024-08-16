// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_store.h"

namespace syncer {

// Handles the shared resources for DataTypeStore and related classes,
// including a shared background sequence runner.
class DataTypeStoreService : public KeyedService {
 public:
  // Returns the root directory under which sync stores data.
  // This doesn't belong here strictly speaking, but it is convenient to
  // centralize all storage-related paths in one class.
  virtual const base::FilePath& GetSyncDataPath() const = 0;

  // Returns a factory to create instances of DataTypeStore with unspecified
  // storage type (i.e. `StorageType::kUnspecified`). May be used from any
  // thread and independently of the lifetime of this object.
  virtual RepeatingDataTypeStoreFactory GetStoreFactory() = 0;

  // Same as above but uses `StorageType::kAccount` for the underlying data
  // storage, which means it's fully isolated from the storage managed via
  // `GetStoreFactory()`.
  virtual RepeatingDataTypeStoreFactory GetStoreFactoryForAccountStorage() = 0;

  virtual scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_H_
