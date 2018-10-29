// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/leveldb/leveldb_service_impl.h"

#include <memory>
#include <utility>

#include "base/sequenced_task_runner.h"
#include "components/services/leveldb/env_mojo.h"
#include "components/services/leveldb/leveldb_database_impl.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace leveldb {

namespace {
void CreateBinding(std::unique_ptr<LevelDBDatabaseImpl> db,
                   leveldb::mojom::LevelDBDatabaseAssociatedRequest request) {
  // The database should be able to close the binding if it gets into an
  // error condition that can't be recovered.
  LevelDBDatabaseImpl* impl = db.get();
  auto binding =
      mojo::MakeStrongAssociatedBinding(std::move(db), std::move(request));
  impl->SetCloseBindingClosure(base::BindOnce(
      &mojo::StrongAssociatedBinding<mojom::LevelDBDatabase>::Close, binding));
}

}  // namespace

LevelDBServiceImpl::LevelDBServiceImpl(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : thread_(new LevelDBMojoProxy(std::move(file_task_runner))) {}

LevelDBServiceImpl::~LevelDBServiceImpl() {}

void LevelDBServiceImpl::Open(
    filesystem::mojom::DirectoryPtr directory,
    const std::string& dbname,
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    leveldb::mojom::LevelDBDatabaseAssociatedRequest database,
    OpenCallback callback) {
  leveldb_env::Options options;
  // the default here to 80 instead of leveldb's default 1000 because we don't
  // want to consume all file descriptors. See
  // https://code.google.com/p/chromium/issues/detail?id=227313#c11 for
  // details.)
  options.max_open_files = 80;

  OpenWithOptions(options, std::move(directory), dbname, memory_dump_id,
                  std::move(database), std::move(callback));
}

void LevelDBServiceImpl::OpenWithOptions(
    const leveldb_env::Options& options,
    filesystem::mojom::DirectoryPtr directory,
    const std::string& dbname,
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    leveldb::mojom::LevelDBDatabaseAssociatedRequest database,
    OpenCallback callback) {
  // Register our directory with the file thread.
  LevelDBMojoProxy::OpaqueDir* dir =
      thread_->RegisterDirectory(std::move(directory));

  std::unique_ptr<MojoEnv> env_mojo(new MojoEnv(thread_, dir));
  leveldb_env::Options open_options = options;
  open_options.env = env_mojo.get();

  std::unique_ptr<leveldb::DB> db;
  leveldb::Status s = leveldb_env::OpenDB(open_options, dbname, &db);

  if (s.ok()) {
    CreateBinding(std::make_unique<LevelDBDatabaseImpl>(
                      std::move(env_mojo), std::move(db), nullptr, open_options,
                      dbname, memory_dump_id),
                  std::move(database));
  }

  std::move(callback).Run(LeveldbStatusToError(s));
}

void LevelDBServiceImpl::OpenInMemory(
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    const std::string& tracking_name,
    leveldb::mojom::LevelDBDatabaseAssociatedRequest database,
    OpenCallback callback) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.

  auto env = leveldb_chrome::NewMemEnv(tracking_name);
  options.env = env.get();

  std::unique_ptr<leveldb::DB> db;
  leveldb::Status s = leveldb_env::OpenDB(options, "", &db);

  if (s.ok()) {
    CreateBinding(std::make_unique<LevelDBDatabaseImpl>(
                      std::move(env), std::move(db), nullptr, options,
                      tracking_name, memory_dump_id),
                  std::move(database));
  }

  std::move(callback).Run(LeveldbStatusToError(s));
}

void LevelDBServiceImpl::Destroy(filesystem::mojom::DirectoryPtr directory,
                                 const std::string& dbname,
                                 DestroyCallback callback) {
  leveldb_env::Options options;
  // Register our directory with the file thread.
  LevelDBMojoProxy::OpaqueDir* dir =
      thread_->RegisterDirectory(std::move(directory));
  std::unique_ptr<MojoEnv> env_mojo(new MojoEnv(thread_, dir));
  options.env = env_mojo.get();
  std::move(callback).Run(
      LeveldbStatusToError(leveldb::DestroyDB(dbname, options)));
}

}  // namespace leveldb
