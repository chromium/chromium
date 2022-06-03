// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_STORE_IMPL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_STORE_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"

namespace leveldb {
class DB;
}

namespace data_reduction_proxy {

// Implementation of |DataStore| using LevelDB.
class DataStoreImpl : public DataStore {
 public:
  explicit DataStoreImpl(const base::FilePath& profile_path);

  DataStoreImpl(const DataStoreImpl&) = delete;
  DataStoreImpl& operator=(const DataStoreImpl&) = delete;

  ~DataStoreImpl() override;

  // Overrides of DataStore.
  void InitializeOnDBThread() override;

  Status Get(base::StringPiece key, std::string* value) override;

  Status Put(const std::map<std::string, std::string>& map) override;

  Status Delete(base::StringPiece key) override;

  // Deletes the LevelDB and recreates it. This method is called if any DB call
  // returns a |CORRUPTED| status or the database is cleared.
  Status RecreateDB() override;

 private:
  // Opens the underlying LevelDB for read and write.
  Status OpenDB();

  // The underlying LevelDB used by this implementation.
  std::unique_ptr<leveldb::DB> db_;

  // Path to the profile using this store.
  const base::FilePath profile_path_;

  base::SequenceChecker sequence_checker_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_STORE_IMPL_H_
