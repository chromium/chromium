// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/blocking_model_type_store_impl.h"
#include "components/sync/model/model_type_store_backend.h"
#include "components/sync/model/model_type_store_impl.h"

namespace syncer {
namespace {

constexpr base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

constexpr base::FilePath::CharType kLevelDBFolderName[] =
    FILE_PATH_LITERAL("LevelDB");

// Initializes ModelTypeStoreBackend, on the backend sequence.
std::optional<ModelError> InitOnBackendSequence(
    const base::FilePath& level_db_path,
    scoped_refptr<ModelTypeStoreBackend> store_backend,
    bool migrate_rl_from_local_to_account) {
  std::vector<std::pair<std::string, std::string>> prefixes_to_migrate;
  if (migrate_rl_from_local_to_account) {
    prefixes_to_migrate.emplace_back(
        BlockingModelTypeStoreImpl::FormatPrefixForModelTypeAndStorageType(
            READING_LIST, StorageType::kUnspecified),
        BlockingModelTypeStoreImpl::FormatPrefixForModelTypeAndStorageType(
            READING_LIST, StorageType::kAccount));
    RecordSyncToSigninMigrationReadingListStep(
        ReadingListMigrationStep::kMigrationStarted);
  }
  return store_backend->Init(level_db_path, prefixes_to_migrate);
}

std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
CreateBlockingModelTypeStoreOnBackendSequence(
    ModelType model_type,
    StorageType storage_type,
    scoped_refptr<ModelTypeStoreBackend> store_backend) {
  BlockingModelTypeStoreImpl* blocking_store = nullptr;
  if (store_backend->IsInitialized()) {
    blocking_store =
        new BlockingModelTypeStoreImpl(model_type, storage_type, store_backend);
  }
  return std::unique_ptr<BlockingModelTypeStoreImpl,
                         base::OnTaskRunnerDeleter /*[]*/>(
      blocking_store, base::OnTaskRunnerDeleter(
                          base::SequencedTaskRunner::GetCurrentDefault()));
}

void ConstructModelTypeStoreOnFrontendSequence(
    ModelType model_type,
    StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    ModelTypeStore::InitCallback callback,
    std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
        blocking_store) {
  if (blocking_store) {
    std::move(callback).Run(
        /*error=*/std::nullopt,
        std::make_unique<ModelTypeStoreImpl>(model_type, storage_type,
                                             std::move(blocking_store),
                                             backend_task_runner));
  } else {
    std::move(callback).Run(
        ModelError(FROM_HERE, "ModelTypeStore backend initialization failed"),
        /*store=*/nullptr);
  }
}

void CreateModelTypeStoreOnFrontendSequence(
    StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<ModelTypeStoreBackend> store_backend,
    ModelType model_type,
    ModelTypeStore::InitCallback callback) {
  // BlockingModelTypeStoreImpl must be instantiated in the backend sequence.
  // This also guarantees that the creation is sequenced with the backend's
  // initialization, since we can't know for sure that InitOnBackendSequence()
  // has already run.
  auto task = base::BindOnce(&CreateBlockingModelTypeStoreOnBackendSequence,
                             model_type, storage_type, store_backend);

  auto reply =
      base::BindOnce(&ConstructModelTypeStoreOnFrontendSequence, model_type,
                     storage_type, backend_task_runner, std::move(callback));

  backend_task_runner->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                  std::move(reply));
}

}  // namespace

ModelTypeStoreServiceImpl::ModelTypeStoreServiceImpl(
    const base::FilePath& base_path,
    PrefService* pref_service)
    : sync_path_(base_path.Append(base::FilePath(kSyncDataFolderName))),
      leveldb_path_(sync_path_.Append(base::FilePath(kLevelDBFolderName))),
      pref_service_(pref_service),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      store_backend_(ModelTypeStoreBackend::CreateUninitialized()) {
  DCHECK(backend_task_runner_);
  bool migrate_rl_from_local_to_account = pref_service_->GetBoolean(
      syncer::prefs::internal::kMigrateReadingListFromLocalToAccount);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitOnBackendSequence, leveldb_path_, store_backend_,
                     migrate_rl_from_local_to_account),
      base::BindOnce(&ModelTypeStoreServiceImpl::BackendInitializationDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

ModelTypeStoreServiceImpl::~ModelTypeStoreServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void ModelTypeStoreServiceImpl::BackendInitializationDone(
    std::optional<ModelError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // If the ReadingList local-to-account migration was performed (or attempted)
  // as part of this initialization, record the outcome.
  if (pref_service_->GetBoolean(
          syncer::prefs::internal::kMigrateReadingListFromLocalToAccount)) {
    if (!error) {
      pref_service_->ClearPref(
          syncer::prefs::internal::kMigrateReadingListFromLocalToAccount);
    }
    RecordSyncToSigninMigrationReadingListStep(
        error ? ReadingListMigrationStep::kMigrationFailed
              : ReadingListMigrationStep::kMigrationFinishedAndPrefCleared);
  }

  base::UmaHistogramBoolean("Sync.ModelTypeStoreBackendInitializationSuccess",
                            !error.has_value());
  if (error) {
    DLOG(ERROR) << "Failed to initialize ModelTypeStore backend: "
                << error->ToString();
  }
}

const base::FilePath& ModelTypeStoreServiceImpl::GetSyncDataPath() const {
  return sync_path_;
}

RepeatingModelTypeStoreFactory ModelTypeStoreServiceImpl::GetStoreFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return base::BindRepeating(&CreateModelTypeStoreOnFrontendSequence,
                             StorageType::kUnspecified, backend_task_runner_,
                             store_backend_);
}

RepeatingModelTypeStoreFactory
ModelTypeStoreServiceImpl::GetStoreFactoryForAccountStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return base::BindRepeating(&CreateModelTypeStoreOnFrontendSequence,
                             StorageType::kAccount, backend_task_runner_,
                             store_backend_);
}

scoped_refptr<base::SequencedTaskRunner>
ModelTypeStoreServiceImpl::GetBackendTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return backend_task_runner_;
}

}  // namespace syncer
