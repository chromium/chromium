// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_store_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
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
#include "components/sync/model/blocking_data_type_store_impl.h"
#include "components/sync/model/data_type_store_backend.h"
#include "components/sync/model/data_type_store_impl.h"

namespace syncer {
namespace {

constexpr base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

constexpr base::FilePath::CharType kLevelDBFolderName[] =
    FILE_PATH_LITERAL("LevelDB");

constexpr const char kLegacyWebApkPrefix[] = "web_apks";

// Initializes DataTypeStoreBackend, on the backend sequence.
std::optional<ModelError> InitOnBackendSequence(
    const base::FilePath& level_db_path,
    scoped_refptr<DataTypeStoreBackend> store_backend,
    bool migrate_rl_from_local_to_account,
    bool wipe_legacy_webapks) {
  base::flat_map<std::string, std::optional<std::string>>
      prefixes_to_update_or_delete;
  if (migrate_rl_from_local_to_account) {
    prefixes_to_update_or_delete.emplace(
        BlockingDataTypeStoreImpl::FormatPrefixForDataTypeAndStorageType(
            READING_LIST, StorageType::kUnspecified),
        BlockingDataTypeStoreImpl::FormatPrefixForDataTypeAndStorageType(
            READING_LIST, StorageType::kAccount));
    RecordSyncToSigninMigrationReadingListStep(
        ReadingListMigrationStep::kMigrationStarted);
  }
  if (wipe_legacy_webapks) {
    // Wipe all WEB_APK data to fix crbug.com/361771496. This won't cause any
    // apps to be uninstalled, such data is only a mirror of the source of
    // truth.
    // std::nullopt in the map below means a deletion.
    // TODO(crbug.com/365978267): Remove migration after enough time.
    prefixes_to_update_or_delete.emplace(kLegacyWebApkPrefix, std::nullopt);
  }
  return store_backend->Init(level_db_path, prefixes_to_update_or_delete);
}

std::unique_ptr<BlockingDataTypeStoreImpl, base::OnTaskRunnerDeleter>
CreateBlockingDataTypeStoreOnBackendSequence(
    DataType data_type,
    StorageType storage_type,
    scoped_refptr<DataTypeStoreBackend> store_backend) {
  BlockingDataTypeStoreImpl* blocking_store = nullptr;
  if (store_backend->IsInitialized()) {
    blocking_store =
        new BlockingDataTypeStoreImpl(data_type, storage_type, store_backend);
  }
  return std::unique_ptr<BlockingDataTypeStoreImpl,
                         base::OnTaskRunnerDeleter /*[]*/>(
      blocking_store, base::OnTaskRunnerDeleter(
                          base::SequencedTaskRunner::GetCurrentDefault()));
}

void ConstructDataTypeStoreOnFrontendSequence(
    DataType data_type,
    StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    DataTypeStore::InitCallback callback,
    std::unique_ptr<BlockingDataTypeStoreImpl, base::OnTaskRunnerDeleter>
        blocking_store) {
  if (blocking_store) {
    std::move(callback).Run(
        /*error=*/std::nullopt,
        std::make_unique<DataTypeStoreImpl>(data_type, storage_type,
                                            std::move(blocking_store),
                                            backend_task_runner));
  } else {
    std::move(callback).Run(
        ModelError(FROM_HERE, "DataTypeStore backend initialization failed"),
        /*store=*/nullptr);
  }
}

void CreateDataTypeStoreOnFrontendSequence(
    StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<DataTypeStoreBackend> store_backend,
    DataType data_type,
    DataTypeStore::InitCallback callback) {
  // BlockingDataTypeStoreImpl must be instantiated in the backend sequence.
  // This also guarantees that the creation is sequenced with the backend's
  // initialization, since we can't know for sure that InitOnBackendSequence()
  // has already run.
  auto task = base::BindOnce(&CreateBlockingDataTypeStoreOnBackendSequence,
                             data_type, storage_type, store_backend);

  auto reply =
      base::BindOnce(&ConstructDataTypeStoreOnFrontendSequence, data_type,
                     storage_type, backend_task_runner, std::move(callback));

  backend_task_runner->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                  std::move(reply));
}

}  // namespace

DataTypeStoreServiceImpl::DataTypeStoreServiceImpl(
    const base::FilePath& base_path,
    PrefService* pref_service)
    : sync_path_(base_path.Append(base::FilePath(kSyncDataFolderName))),
      leveldb_path_(sync_path_.Append(base::FilePath(kLevelDBFolderName))),
      pref_service_(pref_service),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      store_backend_(DataTypeStoreBackend::CreateUninitialized()) {
  DCHECK(backend_task_runner_);
  bool migrate_rl_from_local_to_account = pref_service_->GetBoolean(
      prefs::internal::kMigrateReadingListFromLocalToAccount);
  bool wipe_legacy_webapks =
#if BUILDFLAG(IS_ANDROID)
      !pref_service_->GetBoolean(prefs::internal::kWipedWebAPkDataForMigration);
#else
      false;
#endif  // BUILDFLAG(IS_ANDROID)
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitOnBackendSequence, leveldb_path_, store_backend_,
                     migrate_rl_from_local_to_account, wipe_legacy_webapks),
      base::BindOnce(&DataTypeStoreServiceImpl::BackendInitializationDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

DataTypeStoreServiceImpl::~DataTypeStoreServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void DataTypeStoreServiceImpl::BackendInitializationDone(
    std::optional<ModelError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // If the ReadingList local-to-account migration was performed (or attempted)
  // as part of this initialization, record the outcome.
  if (pref_service_->GetBoolean(
          prefs::internal::kMigrateReadingListFromLocalToAccount)) {
    if (!error) {
      pref_service_->ClearPref(
          prefs::internal::kMigrateReadingListFromLocalToAccount);
    }
    RecordSyncToSigninMigrationReadingListStep(
        error ? ReadingListMigrationStep::kMigrationFailed
              : ReadingListMigrationStep::kMigrationFinishedAndPrefCleared);
  }
#if BUILDFLAG(IS_ANDROID)
  if (!error) {
    pref_service_->SetBoolean(prefs::internal::kWipedWebAPkDataForMigration,
                              true);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::UmaHistogramBoolean("Sync.DataTypeStoreBackendInitializationSuccess",
                            !error.has_value());
  if (error) {
    DLOG(ERROR) << "Failed to initialize DataTypeStore backend: "
                << error->ToString();
  }
}

const base::FilePath& DataTypeStoreServiceImpl::GetSyncDataPath() const {
  return sync_path_;
}

RepeatingDataTypeStoreFactory DataTypeStoreServiceImpl::GetStoreFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return base::BindRepeating(&CreateDataTypeStoreOnFrontendSequence,
                             StorageType::kUnspecified, backend_task_runner_,
                             store_backend_);
}

RepeatingDataTypeStoreFactory
DataTypeStoreServiceImpl::GetStoreFactoryForAccountStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return base::BindRepeating(&CreateDataTypeStoreOnFrontendSequence,
                             StorageType::kAccount, backend_task_runner_,
                             store_backend_);
}

scoped_refptr<base::SequencedTaskRunner>
DataTypeStoreServiceImpl::GetBackendTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return backend_task_runner_;
}

}  // namespace syncer
