// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_SERVICE_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"

namespace syncer {

class ModelTypeStoreBackend;

// Handles the shared resources for ModelTypeStore and related classes,
// including a shared background sequence runner.
class ModelTypeStoreServiceImpl : public ModelTypeStoreService {
 public:
  // |base_path| represents the profile's path.
  explicit ModelTypeStoreServiceImpl(const base::FilePath& base_path);
  ~ModelTypeStoreServiceImpl() override;

  // ModelTypeStoreService:
  const base::FilePath& GetSyncDataPath() const override;
  RepeatingModelTypeStoreFactory GetStoreFactory() override;
  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() override;

 private:
  // The path to the base directory under which sync should store its
  // information.
  const base::FilePath sync_path_;

  // Subdirectory where ModelTypeStore persists the leveldb database.
  const base::FilePath leveldb_path_;

  // The backend sequence or thread.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Constructed in the UI thread, used and destroyed in |backend_task_runner_|.
  const scoped_refptr<ModelTypeStoreBackend> store_backend_;

  SEQUENCE_CHECKER(ui_sequence_checker_);

  base::WeakPtrFactory<ModelTypeStoreServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ModelTypeStoreServiceImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_SERVICE_IMPL_H_
