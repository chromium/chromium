// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_LEVELDB_VALUE_STORE_H_
#define COMPONENTS_VALUE_STORE_LEVELDB_VALUE_STORE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/value_store/lazy_leveldb.h"
#include "components/value_store/value_store.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace value_store {

// Value store area, backed by a leveldb database.
// All methods must be run on the FILE thread.
class LeveldbValueStore : public ValueStore,
                          public LazyLevelDb,
                          public base::trace_event::MemoryDumpProvider {
 public:
  // Creates a database bound to |path|. The underlying database won't be
  // opened (i.e. may not be created) until one of the get/set/etc methods are
  // called - this is because opening the database may fail, and extensions
  // need to be notified of that, but we don't want to permanently give up.
  //
  // Must be created on the FILE thread.
  LeveldbValueStore(const std::string& uma_client_name,
                    const base::FilePath& path);

  // Must be deleted on the FILE thread.
  ~LeveldbValueStore() override;

  LeveldbValueStore(const LeveldbValueStore&) = delete;
  LeveldbValueStore& operator=(const LeveldbValueStore&) = delete;

  // ValueStore implementation.
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;
  ReadResult Get(const std::string& key) override;
  ReadResult Get(const std::vector<std::string>& keys) override;
  ReadResult Get() override;
  WriteResult Set(ValueStore::WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override;
  WriteResult Set(ValueStore::WriteOptions options,
                  const base::Value::Dict& values) override;
  WriteResult Remove(const std::string& key) override;
  WriteResult Remove(const std::vector<std::string>& keys) override;
  WriteResult Clear() override;

  // Write directly to the backing levelDB. Only used for testing to cause
  // corruption in the database.
  bool WriteToDbForTest(leveldb::WriteBatch* batch);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // Adds a setting to a WriteBatch, and logs the change in |changes|. For use
  // with WriteToDb.
  ValueStore::Status AddToBatch(ValueStore::WriteOptions options,
                                const std::string& key,
                                const base::Value& value,
                                leveldb::WriteBatch* batch,
                                ValueStoreChangeList* changes);

  // Commits the changes in |batch| to the database.
  ValueStore::Status WriteToDb(leveldb::WriteBatch* batch);
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_LEVELDB_VALUE_STORE_H_
