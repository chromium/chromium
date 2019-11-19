// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/leveldb_proto/internal/proto/shared_db_metadata.pb.h"
#include "components/leveldb_proto/internal/shared_proto_database_client.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

// Controls a single LevelDB database to be used by many clients, and provides
// a way to get SharedProtoDatabaseClients that allow shared access to the
// underlying single database.
class COMPONENT_EXPORT(LEVELDB_PROTO) SharedProtoDatabase
    : public base::RefCountedThreadSafe<SharedProtoDatabase> {
 public:
  using SharedClientInitCallback =
      base::OnceCallback<void(Enums::InitStatus,
                              SharedDBMetadataProto::MigrationStatus)>;

  // Always returns a SharedProtoDatabaseClient pointer, but that should ONLY
  // be used if the callback returns success.
  std::unique_ptr<SharedProtoDatabaseClient> GetClientForTesting(
      ProtoDbType db_type,
      bool create_if_missing,
      SharedClientInitCallback callback);

  // A version of GetClient that returns the client in a callback instead of
  // giving back a client instance immediately.
  void GetClientAsync(
      ProtoDbType db_type,
      bool create_if_missing,
      base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient>,
                              Enums::InitStatus)> callback);

  void GetDatabaseInitStatusAsync(const std::string& client_db_id,
                                  Callbacks::InitStatusCallback callback);

  void UpdateClientMetadataAsync(
      const std::string& client_db_id,
      SharedDBMetadataProto::MigrationStatus migration_status,
      Callbacks::UpdateCallback callback);

 private:
  friend class base::RefCountedThreadSafe<SharedProtoDatabase>;
  friend class ProtoDatabaseProvider;
  template <typename T>
  friend class ProtoDatabaseImplTest;
  friend class SharedProtoDatabaseTest;
  friend class SharedProtoDatabaseClientTest;
  friend class TestSharedProtoDatabase;

  enum InitState {
    // Initialization hasn't been attempted.
    kNotAttempted,
    // Initialization is in progress, new requests will be enqueued.
    kInProgress,
    // Initialization successful, new requests will return existing DB.
    kSuccess,
    // Initialization failed, new requests will return InitStatus::kError.
    kFailure,
    // Shared database doesn't exist, new requests with create_if_missing ==
    // true will attempt to create it, if create_if_missing == false then will
    // return InitStatus::kInvalidOperation.
    kNotFound,
  };

  struct InitRequest {
    InitRequest(SharedClientInitCallback callback,
                const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                const std::string& client_db_id);

    ~InitRequest();

    SharedClientInitCallback callback;
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    std::string client_db_id;
  };

  // Make sure to give enough time after startup so that we have less chance of
  // affecting startup or navigations.
  static const base::TimeDelta kDelayToClearObsoleteDatabase;

  // Private since we only want to create a singleton of it.
  SharedProtoDatabase(const std::string& client_db_id,
                      const base::FilePath& db_dir);

  virtual ~SharedProtoDatabase();

  void ProcessInitRequests(Enums::InitStatus status);

  std::unique_ptr<SharedProtoDatabaseClient> GetClientInternal(
      ProtoDbType db_type);

  void OnGetClientMetadata(
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      bool success,
      std::unique_ptr<SharedDBMetadataProto> proto);

  // |callback_task_runner| should be the same sequence that Init was called
  // from.
  virtual void Init(
      bool create_if_missing,
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  void InitMetadataDatabase(int attempt, bool corruption);
  void OnMetadataInitComplete(int attempt,
                              bool corruption,
                              leveldb_proto::Enums::InitStatus status);
  void OnGetGlobalMetadata(bool corruption,
                           bool success,
                           std::unique_ptr<SharedDBMetadataProto> proto);
  void OnFinishCorruptionCountWrite(bool success);
  void InitDatabase();
  void OnDatabaseInit(bool create_if_missing, Enums::InitStatus status);
  void CheckCorruptionAndRunInitCallback(
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      Enums::InitStatus status);
  void GetClientMetadataAsync(
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  void OnUpdateCorruptionCountAtInit(bool success);

  void CommitUpdatedGlobalMetadata(Callbacks::UpdateCallback callback);

  void RunInitCallback(
      Callbacks::InitStatusCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  LevelDB* GetLevelDBForTesting() const;

  scoped_refptr<base::SequencedTaskRunner> database_task_runner_for_testing()
      const {
    return task_runner_;
  }

  SEQUENCE_CHECKER(on_task_runner_);

  InitState init_state_ = InitState::kNotAttempted;

  // This TaskRunner is used to properly sequence Init calls and checks for the
  // current init state. When clients request the current InitState as part of
  // their call to their Init function, the request is put into this TaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::FilePath db_dir_;
  std::unique_ptr<LevelDB> db_;
  std::unique_ptr<ProtoLevelDBWrapper> db_wrapper_;

  std::unique_ptr<ProtoDatabase<SharedDBMetadataProto>> metadata_db_wrapper_;
  std::unique_ptr<SharedDBMetadataProto> metadata_;

  // Used to return to the Init callback in the case of an error, so we can
  // report corruptions.
  Enums::InitStatus init_status_ = Enums::InitStatus::kNotInitialized;

  base::queue<std::unique_ptr<InitRequest>> outstanding_init_requests_;
  bool create_if_missing_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharedProtoDatabase);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_H_
