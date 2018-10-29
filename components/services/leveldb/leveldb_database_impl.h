// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_
#define COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_

#include <memory>
#include <vector>

#include "base/trace_event/memory_dump_provider.h"
#include "base/unguessable_token.h"
#include "components/services/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace leveldb {

// The backing to a database object that we pass to our called.
class LevelDBDatabaseImpl : public mojom::LevelDBDatabase,
                            public base::trace_event::MemoryDumpProvider {
 public:
  LevelDBDatabaseImpl(std::unique_ptr<leveldb::Env> environment,
                      std::unique_ptr<leveldb::DB> db,
                      std::unique_ptr<leveldb::Cache> cache,
                      const leveldb_env::Options& options,
                      const std::string& name,
                      base::Optional<base::trace_event::MemoryAllocatorDumpGuid>
                          memory_dump_id);
  ~LevelDBDatabaseImpl() override;

  // Overridden from LevelDBDatabase:
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           PutCallback callback) override;
  void Delete(const std::vector<uint8_t>& key,
              DeleteCallback callback) override;
  void DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                      DeletePrefixedCallback callback) override;
  void RewriteDB(RewriteDBCallback callback) override;
  void Write(std::vector<mojom::BatchedOperationPtr> operations,
             WriteCallback callback) override;
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;
  void GetPrefixed(const std::vector<uint8_t>& key_prefix,
                   GetPrefixedCallback callback) override;
  void CopyPrefixed(const std::vector<uint8_t>& source_key_prefix,
                    const std::vector<uint8_t>& destination_key_prefix,
                    CopyPrefixedCallback callback) override;
  void GetSnapshot(GetSnapshotCallback callback) override;
  void ReleaseSnapshot(const base::UnguessableToken& snapshot) override;
  void GetFromSnapshot(const base::UnguessableToken& snapshot,
                       const std::vector<uint8_t>& key,
                       GetCallback callback) override;
  void NewIterator(NewIteratorCallback callback) override;
  void NewIteratorFromSnapshot(
      const base::UnguessableToken& snapshot,
      NewIteratorFromSnapshotCallback callback) override;
  void ReleaseIterator(const base::UnguessableToken& iterator) override;
  void IteratorSeekToFirst(const base::UnguessableToken& iterator,
                           IteratorSeekToFirstCallback callback) override;
  void IteratorSeekToLast(const base::UnguessableToken& iterator,
                          IteratorSeekToLastCallback callback) override;
  void IteratorSeek(const base::UnguessableToken& iterator,
                    const std::vector<uint8_t>& target,
                    IteratorSeekToLastCallback callback) override;
  void IteratorNext(const base::UnguessableToken& iterator,
                    IteratorNextCallback callback) override;
  void IteratorPrev(const base::UnguessableToken& iterator,
                    IteratorPrevCallback callback) override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Set a closure that will close the mojo connection to this object.
  // The closure will be called if this database becomes unusable e.g.
  // due to a failed rewrite attempt.
  void SetCloseBindingClosure(base::OnceClosure close_binding);

 private:
  // Returns the state of |it| to a caller. Note: This assumes that all the
  // iterator movement methods have the same callback signature. We don't
  // directly reference the underlying type in case of bindings change.
  void ReplyToIteratorMessage(leveldb::Iterator* it,
                              IteratorSeekToFirstCallback callback);

  leveldb::Status DeletePrefixedHelper(const leveldb::Slice& key_prefix,
                                       leveldb::WriteBatch* batch);

  leveldb::Status CopyPrefixedHelper(
      const std::vector<uint8_t>& source_key_prefix,
      const std::vector<uint8_t>& destination_key_prefix,
      leveldb::WriteBatch* batch);

  std::unique_ptr<leveldb::Env> environment_;
  std::unique_ptr<leveldb::Cache> cache_;
  std::unique_ptr<leveldb::DB> db_;
  leveldb_env::Options options_;
  std::string name_;
  base::Optional<base::trace_event::MemoryAllocatorDumpGuid> memory_dump_id_;

  std::map<base::UnguessableToken, const Snapshot*> snapshot_map_;

  // TODO(erg): If we have an existing iterator which depends on a snapshot,
  // and delete the snapshot from the client side, that shouldn't delete the
  // snapshot maybe? At worse it's a DDoS if there's multiple users of the
  // system, but this maybe should be fixed...

  std::map<base::UnguessableToken, Iterator*> iterator_map_;

  base::OnceClosure close_binding_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBDatabaseImpl);
};

}  // namespace leveldb

#endif  // COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_
