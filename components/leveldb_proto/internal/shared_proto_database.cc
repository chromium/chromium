// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto_database_selector.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {

namespace {

const base::FilePath::CharType kMetadataDatabasePath[] =
    FILE_PATH_LITERAL("metadata");

// Number of attempts within a session to open the metadata database. The most
// common errors observed from metrics are IO errors and retries would help
// reduce this. After retries the shared db initialization will fail.
const int kMaxInitMetaDatabaseAttempts = 3;

// The number of consecutive failures when opening shared db after which the db
// is destroyed and created again.
const int kMaxSharedDbFailuresBeforeDestroy = 5;

const char kGlobalMetadataKey[] = "__global";

const char kSharedProtoDatabaseUmaName[] = "SharedDb";

}  // namespace

inline void RunInitStatusCallbackOnCallingSequence(
    SharedProtoDatabase::SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status,
    SharedDBMetadataProto::MigrationStatus migration_status,
    ProtoDatabaseSelector::ProtoDatabaseInitState metric) {
  ProtoDatabaseSelector::RecordInitState(metric);
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status, migration_status));
}

SharedProtoDatabase::InitRequest::InitRequest(
    SharedClientInitCallback callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const std::string& client_db_id)
    : callback(std::move(callback)),
      task_runner(std::move(task_runner)),
      client_db_id(client_db_id) {}

SharedProtoDatabase::InitRequest::~InitRequest() = default;

SharedProtoDatabase::SharedProtoDatabase(const std::string& client_db_id,
                                         const base::FilePath& db_dir)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           // crbug/1006954 and crbug/976223 explain why one of the clients
           // needs run in visible priority. Download DB is always loaded to
           // check for in progress downloads at startup. So, always load shared
           // db in USER_VISIBLE priority.
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      db_dir_(db_dir),
      db_(std::make_unique<LevelDB>(client_db_id.c_str())),
      db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner_)),
      metadata_db_wrapper_(
          ProtoDatabaseProvider::GetUniqueDB<SharedDBMetadataProto>(
              ProtoDbType::SHARED_DB_METADATA,
              db_dir_.Append(base::FilePath(kMetadataDatabasePath)),
              task_runner_)) {
  DETACH_FROM_SEQUENCE(on_task_runner_);
}

// All init functionality runs on the same SequencedTaskRunner, so any caller of
// this after a database Init will receive the correct status of the database.
// PostTaskAndReply is used to ensure that we call the Init callback on its
// original calling thread.
void SharedProtoDatabase::GetDatabaseInitStatusAsync(
    const std::string& client_db_id,
    Callbacks::InitStatusCallback callback) {
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::RunInitCallback, this,
                     std::move(callback),
                     base::SequencedTaskRunner::GetCurrentDefault()));
}

void SharedProtoDatabase::RunInitCallback(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), init_status_));
}

void SharedProtoDatabase::UpdateClientMetadataAsync(
    const std::string& client_db_id,
    SharedDBMetadataProto::MigrationStatus migration_status,
    base::OnceCallback<void(bool)> callback) {
  if (base::SequencedTaskRunner::GetCurrentDefault() != task_runner_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedProtoDatabase::UpdateClientMetadataAsync, this,
                       client_db_id, migration_status, std::move(callback)));
    return;
  }
  auto update_entries = std::make_unique<
      std::vector<std::pair<std::string, SharedDBMetadataProto>>>();
  SharedDBMetadataProto write_proto;
  write_proto.set_corruptions(metadata_->corruptions());
  write_proto.set_migration_status(migration_status);
  update_entries->emplace_back(
      std::make_pair(std::string(client_db_id), write_proto));

  metadata_db_wrapper_->UpdateEntries(
      std::move(update_entries), std::make_unique<std::vector<std::string>>(),
      std::move(callback));
}

void SharedProtoDatabase::GetClientMetadataAsync(
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  // |metadata_db_wrapper_| uses the same TaskRunner as Init and the main
  // DB, so making this call directly here without PostTasking is safe. In
  // addition, GetEntry uses PostTaskAndReply so the callback will be triggered
  // on the calling sequence.
  metadata_db_wrapper_->GetEntry(
      std::string(client_db_id),
      base::BindOnce(&SharedProtoDatabase::OnGetClientMetadata, this,
                     client_db_id, std::move(callback),
                     std::move(callback_task_runner)));
}

// As mentioned above, |current_task_runner| is the appropriate calling sequence
// for the callback since the GetEntry call in GetClientMetadataAsync uses
// PostTaskAndReply.
void SharedProtoDatabase::OnGetClientMetadata(
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool success,
    std::unique_ptr<SharedDBMetadataProto> proto) {
  // If fetching metadata failed, then ignore the error.
  if (!success) {
    RunInitStatusCallbackOnCallingSequence(
        std::move(callback), std::move(callback_task_runner),
        Enums::InitStatus::kOK, SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
        ProtoDatabaseSelector::ProtoDatabaseInitState::
            kSharedDbMetadataLoadFailed);
    return;
  }
  if (!proto || !proto->has_migration_status()) {
    UpdateClientMetadataAsync(
        client_db_id, SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
        base::BindOnce(
            [](SharedClientInitCallback callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
               bool update_success) {
              // Do not care about update success since next time we will reset
              // corruption and migration status to 0.
              RunInitStatusCallbackOnCallingSequence(
                  std::move(callback), std::move(callback_task_runner),
                  Enums::InitStatus::kOK,
                  SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                  ProtoDatabaseSelector::ProtoDatabaseInitState::
                      kSharedDbMetadataWriteFailed);
            },
            std::move(callback), std::move(callback_task_runner)));
    return;
  }
  // If we've made it here, we know that the current status of our database is
  // OK. Make it return corrupt if the metadata disagrees.
  bool is_corrupt = metadata_->corruptions() != proto->corruptions();
  RunInitStatusCallbackOnCallingSequence(
      std::move(callback), std::move(callback_task_runner),
      is_corrupt ? Enums::InitStatus::kCorrupt : Enums::InitStatus::kOK,
      proto->migration_status(),
      is_corrupt ? ProtoDatabaseSelector::ProtoDatabaseInitState::
                       kSharedDbClientCorrupt
                 : ProtoDatabaseSelector::ProtoDatabaseInitState::
                       kSharedDbClientSuccess);
}

void SharedProtoDatabase::CheckCorruptionAndRunInitCallback(
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status) {
  if (init_status_ == Enums::InitStatus::kOK) {
    GetClientMetadataAsync(client_db_id, std::move(callback),
                           std::move(callback_task_runner));
    return;
  }
  RunInitStatusCallbackOnCallingSequence(
      std::move(callback), std::move(callback_task_runner), init_status_,
      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
      ProtoDatabaseSelector::ProtoDatabaseInitState::kSharedLevelDbInitFailure);
}

// Setting |create_if_missing| to false allows us to test whether or not the
// shared database already exists, useful for migrating data from the shared
// database to a unique database if it exists.
// All clients planning to use the shared database should be setting
// |create_if_missing| to true. Setting this to false can result in unexpected
// behaviour since the ordering of Init calls may matter if some calls are made
// with this set to true, and others false.
void SharedProtoDatabase::Init(
    bool create_if_missing,
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  // Try to create the db if any initialization request asked to do so.
  create_if_missing_ = create_if_missing || create_if_missing_;

  switch (init_state_) {
    case InitState::kNotAttempted:
      outstanding_init_requests_.emplace(std::make_unique<InitRequest>(
          std::move(callback), std::move(callback_task_runner), client_db_id));

      init_state_ = InitState::kInProgress;
      // First, try to initialize the metadata database.
      InitMetadataDatabase(0 /* attempt */, false /* corruption */);
      break;

    case InitState::kInProgress:
      outstanding_init_requests_.emplace(std::make_unique<InitRequest>(
          std::move(callback), std::move(callback_task_runner), client_db_id));
      break;

      // If we succeeded previously, just check for corruption status and run
      // init callback.
    case InitState::kSuccess:
      CheckCorruptionAndRunInitCallback(client_db_id, std::move(callback),
                                        std::move(callback_task_runner),
                                        Enums::InitStatus::kOK);
      break;

    // If we previously failed then we run the callback with kError.
    case InitState::kFailure:
      RunInitStatusCallbackOnCallingSequence(
          std::move(callback), std::move(callback_task_runner),
          Enums::InitStatus::kError,
          SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
          ProtoDatabaseSelector::ProtoDatabaseInitState::
              kSharedLevelDbInitFailure);
      break;

    case InitState::kNotFound:
      if (create_if_missing_) {
        // If the shared DB doesn't exist and we should create it if missing,
        // then we skip initializing the metadata DB and initialize the shared
        // DB directly.
        DCHECK(metadata_);
        init_state_ = InitState::kInProgress;
        outstanding_init_requests_.emplace(std::make_unique<InitRequest>(
            std::move(callback), std::move(callback_task_runner),
            client_db_id));
        InitDatabase();
      } else {
        // If the shared DB doesn't exist and we shouldn't create it if missing,
        // then we run the callback with kInvalidOperation (which is not found).
        RunInitStatusCallbackOnCallingSequence(
            std::move(callback), std::move(callback_task_runner),
            Enums::InitStatus::kInvalidOperation,
            SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            ProtoDatabaseSelector::ProtoDatabaseInitState::
                kSharedDbClientMissing);
      }
      break;
  }
}

void SharedProtoDatabase::ProcessInitRequests(Enums::InitStatus status) {
  DCHECK(!outstanding_init_requests_.empty());

  // The pairs are stored as (callback, callback_task_runner).
  while (!outstanding_init_requests_.empty()) {
    auto request = std::move(outstanding_init_requests_.front());
    CheckCorruptionAndRunInitCallback(request->client_db_id,
                                      std::move(request->callback),
                                      std::move(request->task_runner), status);
    outstanding_init_requests_.pop();
  }
}

void SharedProtoDatabase::InitMetadataDatabase(int attempt, bool corruption) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  // TODO: figure out destroy on corruption param
  metadata_db_wrapper_->Init(
      base::BindOnce(&SharedProtoDatabase::OnMetadataInitComplete, this,
                     attempt, corruption));
}

void SharedProtoDatabase::OnMetadataInitComplete(
    int attempt,
    bool corruption,
    leveldb_proto::Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  bool success = status == Enums::kOK;

  if (!success) {
    // We allow some number of attempts to be made to initialize the metadata
    // database because it's crucial for the operation of the shared database.
    // In the event that the metadata DB is corrupt, at least one retry will be
    // made so that we create the DB from scratch again.
    if (attempt < kMaxInitMetaDatabaseAttempts) {
      InitMetadataDatabase(attempt + 1, corruption);
      return;
    }

    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    ProcessInitRequests(init_status_);
    return;
  }

  // Read or initialize the corruption count for this DB. If |corruption| is
  // true, we initialize the counter to 1 right away so that all DBs are forced
  // to treat the shared database as corrupt, we can't know for sure anymore.
  metadata_db_wrapper_->GetEntry(
      std::string(kGlobalMetadataKey),
      base::BindOnce(&SharedProtoDatabase::OnGetGlobalMetadata, this,
                     corruption));
}

void SharedProtoDatabase::OnGetGlobalMetadata(
    bool corruption,
    bool success,
    std::unique_ptr<SharedDBMetadataProto> proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  if (success && proto) {
    // It existed so let's update our internal |corruption_count_|
    metadata_ = std::move(proto);

    if (metadata_->failure_count() >= kMaxSharedDbFailuresBeforeDestroy) {
      ProtoLevelDBWrapper::Destroy(
          db_dir_, /*client_id=*/std::string(), task_runner_,
          base::BindOnce(&SharedProtoDatabase::OnDestroySharedDatabase, this));
      return;
    }

    InitDatabase();
    return;
  }

  // We failed to get the global metadata, so we need to create it for the first
  // time.
  metadata_ = std::make_unique<SharedDBMetadataProto>();
  metadata_->set_corruptions(corruption ? 1U : 0U);
  metadata_->clear_migration_status();
  metadata_->set_failure_count(0);
  CommitUpdatedGlobalMetadata(
      base::BindOnce(&SharedProtoDatabase::OnWriteMetadataAtInit, this));
}

void SharedProtoDatabase::OnWriteMetadataAtInit(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  // TODO(thildebr): Should we retry a few times if we fail this? It feels like
  // if we fail to write this single value something serious happened with the
  // metadata database.
  if (!success) {
    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    ProcessInitRequests(init_status_);
    return;
  }

  InitDatabase();
}

void SharedProtoDatabase::OnDestroySharedDatabase(bool success) {
  if (success) {
    // Destroy database should just delete files in a directory. It fails less
    // often than opening database. If this fails, do not update the failure
    // count and retry destroy in next session and just try to open the database
    // normally.
    metadata_->set_failure_count(0);

    // Try to commit the changes to metadata, but do nothing in case of failure.
    CommitUpdatedGlobalMetadata(base::BindOnce([](bool success) {}));
  }
  if (success) {
    ProtoDatabaseSelector::RecordInitState(
        ProtoDatabaseSelector::ProtoDatabaseInitState::
            kDeletedSharedDbOnRepeatedFailures);
  } else {
    ProtoDatabaseSelector::RecordInitState(
        ProtoDatabaseSelector::ProtoDatabaseInitState::
            kDeletionOfSharedDbFailed);
  }
  InitDatabase();
}

void SharedProtoDatabase::InitDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  auto options = CreateSimpleOptions();
  options.create_if_missing = create_if_missing_;
  db_wrapper_->SetMetricsId(kSharedProtoDatabaseUmaName);
  // |db_wrapper_| uses the same SequencedTaskRunner that Init is called on,
  // so OnDatabaseInit will be called on the same sequence after Init.
  // This means any callers to Init using the same TaskRunner can guarantee that
  // the InitState will be final after Init is called.
  db_wrapper_->InitWithDatabase(
      db_.get(), db_dir_, options, false /* destroy_on_corruption */,
      base::BindOnce(&SharedProtoDatabase::OnDatabaseInit, this,
                     create_if_missing_));
}

void SharedProtoDatabase::OnDatabaseInit(bool create_if_missing,
                                         Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  // Update the corruption counter locally and in the database.
  if (status == Enums::InitStatus::kCorrupt) {
    // TODO(thildebr): Find a clean way to break this into a function to be used
    // by OnGetCorruptionCountEntry.
    // If this fails, we actually send back a failure to the init callback.
    // This may be overkill, but what happens if we don't update this value?
    // Again, it seems like a failure to update here will indicate something
    // serious has gone wrong with the metadata database.
    metadata_->set_corruptions(metadata_->corruptions() + 1);
    metadata_->set_failure_count(metadata_->failure_count() + 1);

    CommitUpdatedGlobalMetadata(base::BindOnce(
        &SharedProtoDatabase::OnUpdateCorruptionCountAtInit, this));
    return;
  }

  // If the previous initialization didn't create the database but a following
  // request tries to create the db. Redo the initialization and create db.
  if (create_if_missing_ && !create_if_missing &&
      status == Enums::InitStatus::kInvalidOperation) {
    DCHECK(init_state_ == InitState::kInProgress ||
           init_state_ == InitState::kNotFound);
    InitDatabase();
    return;
  }

  init_status_ = status;

  switch (status) {
    case Enums::InitStatus::kOK:
      init_state_ = InitState::kSuccess;
      break;
    case Enums::InitStatus::kInvalidOperation:
      DCHECK(!create_if_missing_);
      init_state_ = InitState::kNotFound;
      break;
    case Enums::InitStatus::kError:
    case Enums::InitStatus::kNotInitialized:
    case Enums::InitStatus::kCorrupt:
      init_state_ = InitState::kFailure;
      break;
  }

  ProcessInitRequests(status);

  if (init_state_ == InitState::kSuccess) {
    // Hold on to shared db until the remove operation is done or Shutdown()
    // clears the task.
    Callbacks::UpdateCallback keep_shared_db_alive =
        base::DoNothingWithBoundArgs(base::WrapRefCounted<>(this));
    delete_obsolete_task_.Reset(base::BindOnce(
        &SharedProtoDatabase::DestroyObsoleteSharedProtoDatabaseClients, this,
        std::move(keep_shared_db_alive)));
    base::AutoLock lock(delete_obsolete_delay_lock_);
    task_runner_->PostDelayedTask(FROM_HERE, delete_obsolete_task_.callback(),
                                  delete_obsolete_delay_);
  }
  if (init_state_ == InitState::kSuccess) {
    metadata_->set_failure_count(0);
  } else {
    metadata_->set_failure_count(metadata_->failure_count() + 1);
  }
  // Try to commit the changes to metadata, but do nothing in case of failure.
  CommitUpdatedGlobalMetadata(base::BindOnce([](bool success) {}));
}

void SharedProtoDatabase::Shutdown() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SharedProtoDatabase::Shutdown, this));
    return;
  }
  delete_obsolete_task_.Cancel();
}

void SharedProtoDatabase::OnUpdateCorruptionCountAtInit(bool success) {
  // Return the success value of our write to update the corruption counter.
  // This means that we return kError when the update fails, as a safeguard
  // against clients trying to further persist data when something's gone
  // wrong loading a single metadata proto.
  init_state_ = success ? InitState::kSuccess : InitState::kFailure;
  init_status_ =
      success ? Enums::InitStatus::kCorrupt : Enums::InitStatus::kError;
  ProcessInitRequests(init_status_);
}

void SharedProtoDatabase::CommitUpdatedGlobalMetadata(
    Callbacks::UpdateCallback callback) {
  auto update_entries = std::make_unique<
      std::vector<std::pair<std::string, SharedDBMetadataProto>>>();

  SharedDBMetadataProto write_proto;
  write_proto.CheckTypeAndMergeFrom(*metadata_);
  update_entries->emplace_back(
      std::make_pair(std::string(kGlobalMetadataKey), write_proto));
  metadata_db_wrapper_->UpdateEntries(
      std::move(update_entries), std::make_unique<std::vector<std::string>>(),
      std::move(callback));
}

SharedProtoDatabase::~SharedProtoDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(db_));
  task_runner_->DeleteSoon(FROM_HERE, std::move(metadata_db_wrapper_));
}

void GetClientInitCallback(
    base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient>,
                            Enums::InitStatus)> callback,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    Enums::InitStatus status,
    SharedDBMetadataProto::MigrationStatus migration_status) {
  // |current_task_runner| is valid because Init already takes the current
  // TaskRunner as a parameter and uses that to trigger this callback when it's
  // finished.
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  auto current_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  if (status != Enums::InitStatus::kOK && status != Enums::InitStatus::kCorrupt)
    client.reset();
  // Set migration status of client. The metadata database was already updated.
  if (client)
    client->set_migration_status(migration_status);
  current_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(client), status));
}

void SharedProtoDatabase::GetClientAsync(
    ProtoDbType db_type,
    bool create_if_missing,
    base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient>,
                            Enums::InitStatus)> callback) {
  auto client = GetClientInternal(db_type);
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  auto current_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  SharedProtoDatabaseClient* client_ptr = client.get();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::Init, this, create_if_missing,
                     client_ptr->client_db_id(),
                     base::BindOnce(&GetClientInitCallback, std::move(callback),
                                    std::move(client)),
                     std::move(current_task_runner)));
}

// TODO(thildebr): Need to pass the client name into this call as well, and use
// it with the pending requests too so we can clean up the database.
std::unique_ptr<SharedProtoDatabaseClient>
SharedProtoDatabase::GetClientForTesting(ProtoDbType db_type,
                                         bool create_if_missing,
                                         SharedClientInitCallback callback) {
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  auto current_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto client = GetClientInternal(db_type);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::Init, this, create_if_missing,
                     client->client_db_id(), std::move(callback),
                     std::move(current_task_runner)));
  return client;
}

std::unique_ptr<SharedProtoDatabaseClient>
SharedProtoDatabase::GetClientInternal(ProtoDbType db_type) {
  return base::WrapUnique(new SharedProtoDatabaseClient(
      std::make_unique<ProtoLevelDBWrapper>(task_runner_, db_.get()), db_type,
      this));
}

void SharedProtoDatabase::DestroyObsoleteSharedProtoDatabaseClients(
    Callbacks::UpdateCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  // Create a ProtoLevelDBWrapper just like we create for each client, for
  // deleting data from obsolete clients. It is fine to use the same wrapper to
  // clear data from all clients. This object will be destroyed after clearing
  // data for all these clients.
  auto db_wrapper =
      std::make_unique<ProtoLevelDBWrapper>(task_runner_, db_.get());
  SharedProtoDatabaseClient::DestroyObsoleteSharedProtoDatabaseClients(
      std::move(db_wrapper), std::move(done));
}

void SharedProtoDatabase::SetDeleteObsoleteDelayForTesting(
    base::TimeDelta delay) {
  base::AutoLock lock(delete_obsolete_delay_lock_);
  delete_obsolete_delay_ = delay;
}

LevelDB* SharedProtoDatabase::GetLevelDBForTesting() const {
  return db_.get();
}

}  // namespace leveldb_proto
