// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_IN_MEMORY_DIRECTORY_BACKING_STORE_H_
#define COMPONENTS_SYNC_SYNCABLE_IN_MEMORY_DIRECTORY_BACKING_STORE_H_

#include <string>

#include "base/macros.h"
#include "components/sync/syncable/directory_backing_store.h"

namespace syncer {
namespace syncable {

// This implementation of DirectoryBackingStore is used in tests that do not
// require us to write to a file.  An in-memory sqlite database is much faster
// than an on-disk database, so this can result in significant speedups in our
// unit tests.
//
// An InMemoryDirectoryBackingStore cannot load data from existing databases.
// When an InMemoryDirectoryBackingStore is destroyed, all data stored in this
// database is lost.  If these limitations are a problem for you, consider using
// TestDirectoryBackingStore.
class InMemoryDirectoryBackingStore : public DirectoryBackingStore {
 public:
  explicit InMemoryDirectoryBackingStore(
      const std::string& dir_name,
      const base::RepeatingCallback<std::string()>& cache_guid_generator);
  DirOpenResult Load(Directory::MetahandlesMap* handles_map,
                     MetahandleSet* metahandles_to_purge,
                     Directory::KernelLoadInfo* kernel_load_info) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryDirectoryBackingStore);
};

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_IN_MEMORY_DIRECTORY_BACKING_STORE_H_
