// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_DIRECTORY_BACKING_STORE_H_
#define COMPONENTS_SYNC_TEST_TEST_DIRECTORY_BACKING_STORE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "components/sync/syncable/directory_backing_store.h"

namespace syncer {
namespace syncable {

// This implementation of DirectoryBackingStore does not manage its own
// database.  This makes it more flexible (and more complex) than the
// InMemoryDirectoryBackingStore.
class TestDirectoryBackingStore : public DirectoryBackingStore {
 public:
  // This constructor takes a handle to a database.  The caller maintains
  // ownership of this handle.
  //
  // This is very brittle.  You should not be using this class or this
  // constructor unless you understand and intend to test the
  // DirectoryBackingStore's internals.
  TestDirectoryBackingStore(const std::string& dir_name,
                            sql::Database* connection);
  ~TestDirectoryBackingStore() override;
  DirOpenResult Load(Directory::MetahandlesMap* handles_map,
                     MetahandleSet* metahandles_to_purge,
                     Directory::KernelLoadInfo* kernel_load_info) override;
  bool DeleteEntries(const MetahandleSet& handles);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion67To68);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion68To69);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion69To70);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion70To71);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion71To72);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion72To73);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion73To74);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion74To75);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion75To76);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion76To77);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion77To78);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion78To79);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion79To80);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion80To81);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion81To82);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion82To83);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion83To84);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion84To85);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion85To86);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion86To87);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion87To88);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion88To89);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion89To90);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion90To91);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion91To92);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, DetectInvalidPosition);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, ModelTypeIds);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, Corruption);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, DeleteEntries);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, GenerateCacheGUID);
  FRIEND_TEST_ALL_PREFIXES(MigrationTest, ToCurrentVersion);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateToLatestAndDump);
  friend class MigrationTest;
};

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_DIRECTORY_BACKING_STORE_H_
