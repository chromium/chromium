// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_PROVIDER_H_
#define COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_PROVIDER_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/internal/proto_database_impl.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {

class SharedProtoDatabase;

// Class that provides instances of ProtoDatabase tied to the current
// profile directory.
class COMPONENT_EXPORT(LEVELDB_PROTO) ProtoDatabaseProvider {
 public:
  using GetSharedDBInstanceCallback =
      base::OnceCallback<void(scoped_refptr<SharedProtoDatabase>)>;

  // Retrieve a unique database located in |db_dir|. |db_type| is used to record
  // metrics, converting with
  // SharedProtoDatabaseClientList::ProtoDbTypeToString. |task_runner| is used
  // to run all database operations on.
  // Should only be used in cases when the data is not tied to a specific
  // profile.
  template <typename P, typename T = P>
  static std::unique_ptr<ProtoDatabase<P, T>> GetUniqueDB(
      ProtoDbType db_type,
      const base::FilePath& db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
    return std::make_unique<ProtoDatabaseImpl<P, T>>(db_type, db_dir,
                                                     task_runner);
  }

  // Do not create this directly, instead retrieve from StoragePartition (or
  // BrowserState in iOS) to ensure there's only one per context.
  ProtoDatabaseProvider(const base::FilePath& profile_dir);

  // |db_type|: Each database should have a type specified in ProtoDbType enum.
  // This type is used to index data in the shared database. |unique_db_dir|:
  // the subdirectory this database should live in within the profile directory.
  // |task_runner|: the SequencedTaskRunner to run all database operations on.
  // This isn't used by SharedProtoDatabaseClients since all calls using
  // the SharedProtoDatabase will run on its TaskRunner.
  template <typename P, typename T = P>
  std::unique_ptr<ProtoDatabase<P, T>> GetDB(
      ProtoDbType db_type,
      const base::FilePath& unique_db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);


  virtual ~ProtoDatabaseProvider();

 private:
  friend class TestProtoDatabaseProvider;
  template <typename T_>
  friend class ProtoDatabaseImplTest;
  friend class SharedProtoDatabaseProvider;

  virtual void GetSharedDBInstance(
      GetSharedDBInstanceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  base::FilePath profile_dir_;
  scoped_refptr<SharedProtoDatabase> db_;
  base::Lock get_db_lock_;

  // We store the creation sequence because the SharedProtoDatabaseProvider uses
  // it to make requests to the main provider that rely on WeakPtrs from here,
  // so they're all invalidated/checked on the same sequence.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;

  base::WeakPtrFactory<ProtoDatabaseProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseProvider);
};

template <typename P, typename T>
std::unique_ptr<ProtoDatabase<P, T>> ProtoDatabaseProvider::GetDB(
    ProtoDbType db_type,
    const base::FilePath& unique_db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  return std::make_unique<ProtoDatabaseImpl<P, T>>(
      db_type, unique_db_dir, task_runner,
      base::WrapUnique(new SharedProtoDatabaseProvider(
          client_task_runner_, weak_factory_.GetWeakPtr())));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_PROVIDER_H_
