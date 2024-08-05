// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_IMPL_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"

class PrefService;

namespace syncer {

class DataTypeStoreBackend;

// Handles the shared resources for DataTypeStore and related classes,
// including a shared background sequence runner.
class DataTypeStoreServiceImpl : public DataTypeStoreService {
 public:
  // `base_path` represents the profile's path.
  // `pref_service` must not be null and must outlive this object.
  DataTypeStoreServiceImpl(const base::FilePath& base_path,
                           PrefService* pref_service);

  DataTypeStoreServiceImpl(const DataTypeStoreServiceImpl&) = delete;
  DataTypeStoreServiceImpl& operator=(const DataTypeStoreServiceImpl&) = delete;

  ~DataTypeStoreServiceImpl() override;

  // DataTypeStoreService:
  const base::FilePath& GetSyncDataPath() const override;
  RepeatingDataTypeStoreFactory GetStoreFactory() override;
  RepeatingDataTypeStoreFactory GetStoreFactoryForAccountStorage() override;
  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() override;

 private:
  void BackendInitializationDone(std::optional<ModelError> error);

  // The path to the base directory under which sync should store its
  // information.
  const base::FilePath sync_path_;

  // Subdirectory where DataTypeStore persists the leveldb database.
  const base::FilePath leveldb_path_;

  const raw_ptr<PrefService> pref_service_;

  // The backend sequence or thread.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Constructed on the UI thread, used on |backend_task_runner_| and destroyed
  // on any sequence.
  const scoped_refptr<DataTypeStoreBackend> store_backend_;

  SEQUENCE_CHECKER(ui_sequence_checker_);

  base::WeakPtrFactory<DataTypeStoreServiceImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_IMPL_H_
