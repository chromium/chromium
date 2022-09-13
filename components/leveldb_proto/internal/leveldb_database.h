// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_LEVELDB_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_LEVELDB_DATABASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace base {
class FilePath;
class HistogramBase;
}  // namespace base

namespace leveldb {
class DB;
class Env;
}  // namespace leveldb

namespace leveldb_proto {

// Interacts with the LevelDB third party module.
// Once constructed, function calls and destruction should all occur on the
// same thread (not necessarily the same as the constructor).
class COMPONENT_EXPORT(LEVELDB_PROTO) LevelDB {
 public:
  // Decides the next action based on the key.
  //
  // Returns `kSkipAndStop` when |while_callback| is false. Skips entries when
  // |filter| is false.  Doesn't ever return `kLoadAndStop`.
  static Enums::KeyIteratorAction ComputeIteratorAction(
      const KeyFilter& while_callback,
      const KeyFilter& filter,
      const std::string& key);

  // Constructor. Does *not* open a leveldb - only initialize this class.
  // |client_name| is the name of the "client" that owns this instance. Used
  // for UMA statics as so: LevelDB.<value>.<client name>. It is best to not
  // change once shipped.
  explicit LevelDB(const char* client_name);

  LevelDB(const LevelDB&) = delete;
  LevelDB& operator=(const LevelDB&) = delete;

  virtual ~LevelDB();

  // Initializes a leveldb with the given options. If |database_dir| is
  // empty, this opens an in-memory db.
  virtual bool Init(const base::FilePath& database_dir,
                    const leveldb_env::Options& options);
  virtual leveldb::Status Init(const base::FilePath& database_dir,
                               const leveldb_env::Options& options,
                               bool destroy_on_corruption);

  virtual bool Save(const base::StringPairs& pairs_to_save,
                    const std::vector<std::string>& keys_to_remove,
                    leveldb::Status* status);

  virtual bool UpdateWithRemoveFilter(const base::StringPairs& entries_to_save,
                                      const KeyFilter& delete_key_filter,
                                      leveldb::Status* status);
  virtual bool UpdateWithRemoveFilter(const base::StringPairs& entries_to_save,
                                      const KeyFilter& delete_key_filter,
                                      const std::string& target_prefix,
                                      leveldb::Status* status);

  virtual bool Load(std::vector<std::string>* entries);
  virtual bool LoadWithFilter(const KeyFilter& filter,
                              std::vector<std::string>* entries);
  virtual bool LoadWithFilter(const KeyFilter& filter,
                              std::vector<std::string>* entries,
                              const leveldb::ReadOptions& options,
                              const std::string& target_prefix);

  virtual bool LoadKeysAndEntries(
      std::map<std::string, std::string>* keys_entries);
  virtual bool LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      std::map<std::string, std::string>* keys_entries);
  virtual bool LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      std::map<std::string, std::string>* keys_entries,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix);

  // Retrieves consecutive keys and values.
  //
  // Starts at or after |start_key|. Loads entries when |controller| returns
  // `kLoadAndContinue` or `kLoadAndStop`. Finishes when |controller| returns
  // `kLoadAndStop` or `kSkipAndStop`.
  virtual bool LoadKeysAndEntriesWhile(
      std::map<std::string, std::string>* keys_entries,
      const leveldb::ReadOptions& options,
      const std::string& start_key,
      const KeyIteratorController& controller);

  // Retrieves consecutive keys and values.
  //
  // Starts at or after |start_key|. Skips entries when |filter| returns
  // `false`. Stops scanning and skips the entry when |while_callback| returns
  // `false`.
  virtual bool LoadKeysAndEntriesWhile(
      const KeyFilter& filter,
      std::map<std::string, std::string>* keys_entries,
      const leveldb::ReadOptions& options,
      const std::string& start_key,
      const KeyFilter& while_callback);

  virtual bool LoadKeys(std::vector<std::string>* keys);
  virtual bool LoadKeys(const std::string& target_prefix,
                        std::vector<std::string>* keys);

  virtual bool Get(const std::string& key,
                   bool* found,
                   std::string* entry,
                   leveldb::Status* status);
  // Close (if currently open) and then destroy (i.e. delete) the database
  // directory.
  virtual leveldb::Status Destroy();

  // Returns true if we successfully read the approximate memory usage property
  // from the LevelDB.
  bool GetApproximateMemoryUse(uint64_t* approx_mem);

 private:
  DFAKE_MUTEX(thread_checker_);

  // The declaration order of these members matters: |db_| depends on |env_| and
  // therefore has to be destructed first.
  std::unique_ptr<leveldb::Env> env_;
  std::unique_ptr<leveldb::DB> db_;
  base::FilePath database_dir_;
  leveldb_env::Options open_options_;
  raw_ptr<base::HistogramBase> approx_memtable_mem_histogram_;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_LEVELDB_DATABASE_H_
