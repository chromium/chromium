// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/directory_unittest.h"

#include <stddef.h>

#include <cstdlib>

#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "components/sync/base/mock_unrecoverable_error_handler.h"
#include "components/sync/syncable/syncable_proto_util.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/test/engine/test_syncable_utils.h"
#include "components/sync/test/test_directory_backing_store.h"

using base::ExpectDictBooleanValue;
using base::ExpectDictStringValue;

namespace syncer {

namespace syncable {

namespace {

bool IsLegalNewParent(const Entry& a, const Entry& b) {
  return IsLegalNewParent(a.trans(), a.GetId(), b.GetId());
}

void PutDataAsBookmarkFavicon(WriteTransaction* wtrans,
                              MutableEntry* e,
                              const char* bytes,
                              size_t bytes_length) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_url("http://demo/");
  specifics.mutable_bookmark()->set_favicon(bytes, bytes_length);
  e->PutSpecifics(specifics);
}

void ExpectDataFromBookmarkFaviconEquals(BaseTransaction* trans,
                                         Entry* e,
                                         const char* bytes,
                                         size_t bytes_length) {
  ASSERT_TRUE(e->good());
  ASSERT_TRUE(e->GetSpecifics().has_bookmark());
  ASSERT_EQ("http://demo/", e->GetSpecifics().bookmark().url());
  ASSERT_EQ(std::string(bytes, bytes_length),
            e->GetSpecifics().bookmark().favicon());
}

}  // namespace

const char SyncableDirectoryTest::kDirectoryName[] = "Foo";

SyncableDirectoryTest::SyncableDirectoryTest() {}

SyncableDirectoryTest::~SyncableDirectoryTest() {}

void SyncableDirectoryTest::SetUp() {
  ASSERT_TRUE(connection_.OpenInMemory());
  ASSERT_EQ(OPENED_NEW, ReopenDirectory());
}

void SyncableDirectoryTest::TearDown() {
  if (dir_)
    dir_->SaveChanges();
  dir_.reset();
}

DirOpenResult SyncableDirectoryTest::ReopenDirectory() {
  // Use a TestDirectoryBackingStore and sql::Database so we can have test
  // data persist across Directory object lifetimes while getting the
  // performance benefits of not writing to disk.
  dir_ = std::make_unique<Directory>(
      std::make_unique<TestDirectoryBackingStore>(kDirectoryName, &connection_),
      MakeWeakHandle(handler_.GetWeakPtr()), base::Closure(), nullptr);

  DirOpenResult open_result =
      dir_->Open(kDirectoryName, &delegate_, NullTransactionObserver());

  if (open_result != OPENED_NEW && open_result != OPENED_EXISTING) {
    dir_.reset();
  } else {
    dir_->set_cache_guid(dir_->legacy_cache_guid());
  }

  return open_result;
}

// Creates an empty entry and sets the ID field to a default one.
void SyncableDirectoryTest::CreateEntry(const ModelType& model_type,
                                        const std::string& entryname) {
  CreateEntry(model_type, entryname, TestIdFactory::FromNumber(-99));
}

// Creates an empty entry and sets the ID field to id.
void SyncableDirectoryTest::CreateEntry(const ModelType& model_type,
                                        const std::string& entryname,
                                        const int id) {
  CreateEntry(model_type, entryname, TestIdFactory::FromNumber(id));
}

void SyncableDirectoryTest::CreateEntry(const ModelType& model_type,
                                        const std::string& entryname,
                                        const Id& id) {
  WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry me(&wtrans, CREATE, model_type, wtrans.root_id(), entryname);
  ASSERT_TRUE(me.good());
  me.PutId(id);
  me.PutIsUnsynced(true);
}

void SyncableDirectoryTest::DeleteEntry(const Id& id) {
  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry entry(&trans, GET_BY_ID, id);
  ASSERT_TRUE(entry.good());
  entry.PutIsDel(true);
}

DirOpenResult SyncableDirectoryTest::SimulateSaveAndReloadDir() {
  if (!dir_->SaveChanges())
    return FAILED_IN_UNITTEST;

  return ReopenDirectory();
}

DirOpenResult SyncableDirectoryTest::SimulateCrashAndReloadDir() {
  return ReopenDirectory();
}

void SyncableDirectoryTest::GetAllMetaHandles(BaseTransaction* trans,
                                              MetahandleSet* result) {
  dir_->GetAllMetaHandles(trans, result);
}

void SyncableDirectoryTest::CheckPurgeEntriesWithTypeInSucceeded(
    ModelTypeSet types_to_purge,
    bool before_reload) {
  SCOPED_TRACE(testing::Message("Before reload: ") << before_reload);
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    MetahandleSet all_set;
    dir_->GetAllMetaHandles(&trans, &all_set);
    EXPECT_EQ(4U, all_set.size());
    if (before_reload)
      EXPECT_EQ(6U, dir_->kernel()->metahandles_to_purge.size());
    for (auto iter = all_set.begin(); iter != all_set.end(); ++iter) {
      Entry e(&trans, GET_BY_HANDLE, *iter);
      const ModelType local_type = e.GetModelType();
      const ModelType server_type = e.GetServerModelType();

      // Note the dance around incrementing |it|, since we sometimes erase().
      if ((IsRealDataType(local_type) && types_to_purge.Has(local_type)) ||
          (IsRealDataType(server_type) && types_to_purge.Has(server_type))) {
        FAIL() << "Illegal type should have been deleted.";
      }
    }
  }

  for (ModelType type : types_to_purge) {
    EXPECT_FALSE(dir_->InitialSyncEndedForType(type));
    sync_pb::DataTypeProgressMarker progress;
    dir_->GetDownloadProgress(type, &progress);
    EXPECT_EQ("", progress.token());

    ReadTransaction trans(FROM_HERE, dir_.get());
    sync_pb::DataTypeContext context;
    dir_->GetDataTypeContext(&trans, type, &context);
    EXPECT_TRUE(context.SerializeAsString().empty());
  }
  EXPECT_FALSE(types_to_purge.Has(BOOKMARKS));
  EXPECT_TRUE(dir_->InitialSyncEndedForType(BOOKMARKS));
}

bool SyncableDirectoryTest::IsInDirtyMetahandles(int64_t metahandle) {
  return 1 == dir_->kernel()->dirty_metahandles.count(metahandle);
}

bool SyncableDirectoryTest::IsInMetahandlesToPurge(int64_t metahandle) {
  return 1 == dir_->kernel()->metahandles_to_purge.count(metahandle);
}

std::unique_ptr<Directory>& SyncableDirectoryTest::dir() {
  return dir_;
}

DirectoryChangeDelegate* SyncableDirectoryTest::directory_change_delegate() {
  return &delegate_;
}

Encryptor* SyncableDirectoryTest::encryptor() {
  return &encryptor_;
}

TestUnrecoverableErrorHandler*
SyncableDirectoryTest::unrecoverable_error_handler() {
  return &handler_;
}

void SyncableDirectoryTest::ValidateEntry(BaseTransaction* trans,
                                          int64_t id,
                                          bool check_name,
                                          const std::string& name,
                                          int64_t base_version,
                                          int64_t server_version,
                                          bool is_del) {
  Entry e(trans, GET_BY_ID, TestIdFactory::FromNumber(id));
  ASSERT_TRUE(e.good());
  if (check_name)
    ASSERT_EQ(name, e.GetNonUniqueName());
  ASSERT_EQ(base_version, e.GetBaseVersion());
  ASSERT_EQ(server_version, e.GetServerVersion());
  ASSERT_EQ(is_del, e.GetIsDel());
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsMetahandlesToPurge) {
  const int metas_to_create = 50;
  MetahandleSet expected_purges;
  MetahandleSet all_handles;
  {
    dir()->SetDownloadProgress(BOOKMARKS, BuildProgress(BOOKMARKS));
    dir()->SetDownloadProgress(PREFERENCES, BuildProgress(PREFERENCES));
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (int i = 0; i < metas_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      e.PutIsUnsynced(true);
      sync_pb::EntitySpecifics specs;
      if (i % 2 == 0) {
        AddDefaultFieldValue(BOOKMARKS, &specs);
        expected_purges.insert(e.GetMetahandle());
        all_handles.insert(e.GetMetahandle());
      } else {
        AddDefaultFieldValue(PREFERENCES, &specs);
        all_handles.insert(e.GetMetahandle());
      }
      e.PutSpecifics(specs);
      e.PutServerSpecifics(specs);
    }
  }

  ModelTypeSet to_purge(BOOKMARKS);
  dir()->PurgeEntriesWithTypeIn(to_purge, ModelTypeSet(), ModelTypeSet());

  Directory::SaveChangesSnapshot snapshot1;
  base::AutoLock scoped_lock(dir()->kernel()->save_changes_mutex);
  dir()->TakeSnapshotForSaveChanges(&snapshot1);
  EXPECT_EQ(expected_purges, snapshot1.metahandles_to_purge);

  to_purge.Clear();
  to_purge.Put(PREFERENCES);
  dir()->PurgeEntriesWithTypeIn(to_purge, ModelTypeSet(), ModelTypeSet());

  dir()->HandleSaveChangesFailure(snapshot1);

  Directory::SaveChangesSnapshot snapshot2;
  dir()->TakeSnapshotForSaveChanges(&snapshot2);
  EXPECT_EQ(all_handles, snapshot2.metahandles_to_purge);
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsAllDirtyHandlesTest) {
  const int metahandles_to_create = 100;
  std::vector<int64_t> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.GetMetahandle());
      e.PutIsUnsynced(true);
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel()->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    ASSERT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (auto i = snapshot.dirty_metas.begin(); i != snapshot.dirty_metas.end();
         ++i) {
      ASSERT_TRUE((*i)->is_dirty());
    }
    dir()->VacuumAfterSaveChanges(snapshot);
  }
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    std::vector<int64_t> new_dirty_metahandles;
    for (std::vector<int64_t>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end(); ++i) {
      // Change existing entries to directories to dirty them.
      MutableEntry e1(&trans, GET_BY_HANDLE, *i);
      e1.PutIsDir(true);
      e1.PutIsUnsynced(true);
      // Add new entries
      MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), "bar");
      e2.PutIsUnsynced(true);
      new_dirty_metahandles.push_back(e2.GetMetahandle());
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
                                      new_dirty_metahandles.begin(),
                                      new_dirty_metahandles.end());
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel()->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (auto i = snapshot.dirty_metas.begin(); i != snapshot.dirty_metas.end();
         ++i) {
      EXPECT_TRUE((*i)->is_dirty());
    }
    dir()->VacuumAfterSaveChanges(snapshot);
  }
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsOnlyDirtyHandlesTest) {
  const int metahandles_to_create = 100;

  // half of 2 * metahandles_to_create
  const unsigned int number_changed = 100u;
  std::vector<int64_t> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.GetMetahandle());
      e.PutIsUnsynced(true);
    }
  }
  dir()->SaveChanges();
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    std::vector<int64_t> new_dirty_metahandles;
    for (std::vector<int64_t>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end(); ++i) {
      // Change existing entries to directories to dirty them.
      MutableEntry e1(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e1.good());
      e1.PutIsDir(true);
      e1.PutIsUnsynced(true);
      // Add new entries
      MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), "bar");
      e2.PutIsUnsynced(true);
      new_dirty_metahandles.push_back(e2.GetMetahandle());
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
                                      new_dirty_metahandles.begin(),
                                      new_dirty_metahandles.end());
  }
  dir()->SaveChanges();
  // Don't make any changes whatsoever and ensure nothing comes back.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (std::vector<int64_t>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end(); ++i) {
      MutableEntry e(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e.good());
      // We aren't doing anything to dirty these entries.
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel()->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there are no dirty_metahandles.
    EXPECT_EQ(0u, snapshot.dirty_metas.size());
    dir()->VacuumAfterSaveChanges(snapshot);
  }
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    bool should_change = false;
    for (std::vector<int64_t>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end(); ++i) {
      // Maybe change entries by flipping IS_DIR.
      MutableEntry e(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e.good());
      should_change = !should_change;
      if (should_change) {
        bool not_dir = !e.GetIsDir();
        e.PutIsDir(not_dir);
        e.PutIsUnsynced(true);
      }
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel()->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each changed metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(number_changed, snapshot.dirty_metas.size());
    for (auto i = snapshot.dirty_metas.begin(); i != snapshot.dirty_metas.end();
         ++i) {
      EXPECT_TRUE((*i)->is_dirty());
    }
    dir()->VacuumAfterSaveChanges(snapshot);
  }
}

TEST_F(SyncableDirectoryTest, TestPurgeDeletedEntriesOnReload) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(PREFERENCES, &specifics);

  const int kClientCount = 2;
  const int kServerCount = 5;
  const int kTestCount = kClientCount + kServerCount;
  int64_t handles[kTestCount];

  // The idea is to recreate various combinations of IDs, IS_DEL,
  // IS_UNSYNCED, and IS_UNAPPLIED_UPDATE flags to test all combinations
  // for DirectoryBackingStore::SafeToPurgeOnLoading.
  // 0: client ID, IS_DEL, IS_UNSYNCED
  // 1: client ID, IS_UNSYNCED
  // 2: server ID, IS_DEL, IS_UNSYNCED, IS_UNAPPLIED_UPDATE
  // 3: server ID, IS_DEL, IS_UNSYNCED
  // 4: server ID, IS_DEL, IS_UNAPPLIED_UPDATE
  // 5: server ID, IS_DEL
  // 6: server ID
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    for (int i = 0; i < kTestCount; i++) {
      std::string name = base::StringPrintf("item%d", i);
      MutableEntry item(&trans, CREATE, PREFERENCES, trans.root_id(), name);
      ASSERT_TRUE(item.good());

      handles[i] = item.GetMetahandle();

      if (i < kClientCount) {
        item.PutId(TestIdFactory::FromNumber(i - kClientCount));
      } else {
        item.PutId(TestIdFactory::FromNumber(i));
      }

      item.PutUniqueClientTag(name);
      item.PutIsUnsynced(true);
      item.PutSpecifics(specifics);
      item.PutServerSpecifics(specifics);

      if (i >= kClientCount) {
        item.PutBaseVersion(10);
        item.PutServerVersion(10);
      }

      // Set flags
      if (i != 1 && i != 6)
        item.PutIsDel(true);

      if (i >= 4)
        item.PutIsUnsynced(false);

      if (i == 2 || i == 4)
        item.PutIsUnappliedUpdate(true);
    }
  }
  ASSERT_EQ(OPENED_EXISTING, SimulateSaveAndReloadDir());

  // Expect items 0 and 5 to be purged according to
  // DirectoryBackingStore::SafeToPurgeOnLoading:
  // - Item 0 is an item with IS_DEL flag and client ID.
  // - Item 5 is an item with IS_DEL flag which has both
  //   IS_UNSYNCED and IS_UNAPPLIED_UPDATE unset.
  std::vector<int64_t> expected_purged;
  expected_purged.push_back(0);
  expected_purged.push_back(5);

  std::vector<int64_t> actually_purged;
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    for (int i = 0; i < kTestCount; i++) {
      Entry item(&trans, GET_BY_HANDLE, handles[i]);
      if (!item.good()) {
        actually_purged.push_back(i);
      }
    }
  }

  EXPECT_EQ(expected_purged, actually_purged);
}

TEST_F(SyncableDirectoryTest, TestBasicLookupNonExistantID) {
  ReadTransaction rtrans(FROM_HERE, dir().get());
  Entry e(&rtrans, GET_BY_ID, TestIdFactory::FromNumber(-99));
  ASSERT_FALSE(e.good());
}

TEST_F(SyncableDirectoryTest, TestBasicLookupValidID) {
  CreateEntry(BOOKMARKS, "rtc");
  ReadTransaction rtrans(FROM_HERE, dir().get());
  Entry e(&rtrans, GET_BY_ID, TestIdFactory::FromNumber(-99));
  ASSERT_TRUE(e.good());
}

TEST_F(SyncableDirectoryTest, TestDelete) {
  std::string name = "peanut butter jelly time";
  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e1.good());
  e1.PutIsDel(true);
  MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e2.good());
  e2.PutIsDel(true);
  MutableEntry e3(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e3.good());
  e3.PutIsDel(true);

  e1.PutIsDel(false);
  e2.PutIsDel(false);
  e3.PutIsDel(false);

  e1.PutIsDel(true);
  e2.PutIsDel(true);
  e3.PutIsDel(true);
}

TEST_F(SyncableDirectoryTest, TestGetUnsynced) {
  Directory::Metahandles handles;
  int64_t handle1, handle2;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(0u, handles.size());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.GetMetahandle();
    e1.PutBaseVersion(1);
    e1.PutIsDir(true);
    e1.PutId(TestIdFactory::FromNumber(101));

    MutableEntry e2(&trans, CREATE, BOOKMARKS, e1.GetId(), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.GetMetahandle();
    e2.PutBaseVersion(1);
    e2.PutId(TestIdFactory::FromNumber(102));
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(0u, handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.PutIsUnsynced(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(1u, handles.size());
    ASSERT_EQ(handle1, handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.PutIsUnsynced(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(2u, handles.size());
    if (handle1 == handles[0]) {
      ASSERT_EQ(handle2, handles[1]);
    } else {
      ASSERT_EQ(handle2, handles[0]);
      ASSERT_EQ(handle1, handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    ASSERT_TRUE(e5.GetIsUnsynced());
    ASSERT_TRUE(e5.PutIsUnsynced(false));
    ASSERT_FALSE(e5.GetIsUnsynced());
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(1u, handles.size());
    ASSERT_EQ(handle2, handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, TestGetUnappliedUpdates) {
  std::vector<int64_t> handles;
  int64_t handle1, handle2;
  const FullModelTypeSet all_types = FullModelTypeSet::All();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_EQ(0u, handles.size());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.GetMetahandle();
    e1.PutIsUnappliedUpdate(false);
    e1.PutBaseVersion(1);
    e1.PutId(TestIdFactory::FromNumber(101));
    e1.PutIsDir(true);

    MutableEntry e2(&trans, CREATE, BOOKMARKS, e1.GetId(), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.GetMetahandle();
    e2.PutIsUnappliedUpdate(false);
    e2.PutBaseVersion(1);
    e2.PutId(TestIdFactory::FromNumber(102));
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_EQ(0u, handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.PutIsUnappliedUpdate(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_EQ(1u, handles.size());
    ASSERT_EQ(handle1, handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.PutIsUnappliedUpdate(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_EQ(2u, handles.size());
    if (handle1 == handles[0]) {
      ASSERT_EQ(handle2, handles[1]);
    } else {
      ASSERT_EQ(handle2, handles[0]);
      ASSERT_EQ(handle1, handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    e5.PutIsUnappliedUpdate(false);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_EQ(1u, handles.size());
    ASSERT_EQ(handle2, handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, DeleteBug_531383) {
  // Try to evoke a check failure...
  TestIdFactory id_factory;
  int64_t grandchild_handle;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, id_factory.root(), "Bob");
    ASSERT_TRUE(parent.good());
    parent.PutIsDir(true);
    parent.PutId(id_factory.NewServerId());
    parent.PutBaseVersion(1);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.GetId(), "Bob");
    ASSERT_TRUE(child.good());
    child.PutIsDir(true);
    child.PutId(id_factory.NewServerId());
    child.PutBaseVersion(1);
    MutableEntry grandchild(&wtrans, CREATE, BOOKMARKS, child.GetId(), "Bob");
    ASSERT_TRUE(grandchild.good());
    grandchild.PutId(id_factory.NewServerId());
    grandchild.PutBaseVersion(1);
    grandchild.PutIsDel(true);
    MutableEntry twin(&wtrans, CREATE, BOOKMARKS, child.GetId(), "Bob");
    ASSERT_TRUE(twin.good());
    twin.PutIsDel(true);
    grandchild.PutIsDel(false);

    grandchild_handle = grandchild.GetMetahandle();
  }
  dir()->SaveChanges();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry grandchild(&wtrans, GET_BY_HANDLE, grandchild_handle);
    grandchild.PutIsDel(true);  // Used to CHECK fail here.
  }
}

TEST_F(SyncableDirectoryTest, TestIsLegalNewParent) {
  TestIdFactory id_factory;
  WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
  Entry root(&wtrans, GET_BY_ID, id_factory.root());
  ASSERT_TRUE(root.good());
  MutableEntry parent(&wtrans, CREATE, BOOKMARKS, root.GetId(), "Bob");
  ASSERT_TRUE(parent.good());
  parent.PutIsDir(true);
  parent.PutId(id_factory.NewServerId());
  parent.PutBaseVersion(1);
  MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.GetId(), "Bob");
  ASSERT_TRUE(child.good());
  child.PutIsDir(true);
  child.PutId(id_factory.NewServerId());
  child.PutBaseVersion(1);
  MutableEntry grandchild(&wtrans, CREATE, BOOKMARKS, child.GetId(), "Bob");
  ASSERT_TRUE(grandchild.good());
  grandchild.PutId(id_factory.NewServerId());
  grandchild.PutBaseVersion(1);

  MutableEntry parent2(&wtrans, CREATE, BOOKMARKS, root.GetId(), "Pete");
  ASSERT_TRUE(parent2.good());
  parent2.PutIsDir(true);
  parent2.PutId(id_factory.NewServerId());
  parent2.PutBaseVersion(1);
  MutableEntry child2(&wtrans, CREATE, BOOKMARKS, parent2.GetId(), "Pete");
  ASSERT_TRUE(child2.good());
  child2.PutIsDir(true);
  child2.PutId(id_factory.NewServerId());
  child2.PutBaseVersion(1);
  MutableEntry grandchild2(&wtrans, CREATE, BOOKMARKS, child2.GetId(), "Pete");
  ASSERT_TRUE(grandchild2.good());
  grandchild2.PutId(id_factory.NewServerId());
  grandchild2.PutBaseVersion(1);
  // resulting tree
  //           root
  //           /  |
  //     parent    parent2
  //          |    |
  //      child    child2
  //          |    |
  // grandchild    grandchild2
  ASSERT_TRUE(IsLegalNewParent(child, root));
  ASSERT_TRUE(IsLegalNewParent(child, parent));
  ASSERT_FALSE(IsLegalNewParent(child, child));
  ASSERT_FALSE(IsLegalNewParent(child, grandchild));
  ASSERT_TRUE(IsLegalNewParent(child, parent2));
  ASSERT_TRUE(IsLegalNewParent(child, grandchild2));
  ASSERT_FALSE(IsLegalNewParent(parent, grandchild));
  ASSERT_FALSE(IsLegalNewParent(root, grandchild));
  ASSERT_FALSE(IsLegalNewParent(parent, grandchild));
}

TEST_F(SyncableDirectoryTest, TestEntryIsInFolder) {
  // Create a subdir and an entry.
  int64_t entry_handle;
  syncable::Id folder_id;
  syncable::Id entry_id;
  std::string entry_name = "entry";

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "folder");
    ASSERT_TRUE(folder.good());
    folder.PutIsDir(true);
    EXPECT_TRUE(folder.PutIsUnsynced(true));
    folder_id = folder.GetId();

    MutableEntry entry(&trans, CREATE, BOOKMARKS, folder.GetId(), entry_name);
    ASSERT_TRUE(entry.good());
    entry_handle = entry.GetMetahandle();
    entry.PutIsUnsynced(true);
    entry_id = entry.GetId();
  }

  // Make sure we can find the entry in the folder.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), entry_name));
    EXPECT_EQ(1, CountEntriesWithName(&trans, folder_id, entry_name));

    Entry entry(&trans, GET_BY_ID, entry_id);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(entry_handle, entry.GetMetahandle());
    EXPECT_EQ(entry_name, entry.GetNonUniqueName());
    EXPECT_EQ(folder_id, entry.GetParentId());
  }
}

TEST_F(SyncableDirectoryTest, TestParentIdIndexUpdate) {
  std::string child_name = "child";

  WriteTransaction wt(FROM_HERE, UNITTEST, dir().get());
  MutableEntry parent_folder(&wt, CREATE, BOOKMARKS, wt.root_id(), "folder1");
  parent_folder.PutIsUnsynced(true);
  parent_folder.PutIsDir(true);

  MutableEntry parent_folder2(&wt, CREATE, BOOKMARKS, wt.root_id(), "folder2");
  parent_folder2.PutIsUnsynced(true);
  parent_folder2.PutIsDir(true);

  MutableEntry child(&wt, CREATE, BOOKMARKS, parent_folder.GetId(), child_name);
  child.PutIsDir(true);
  child.PutIsUnsynced(true);

  ASSERT_TRUE(child.good());

  EXPECT_EQ(0, CountEntriesWithName(&wt, wt.root_id(), child_name));
  EXPECT_EQ(parent_folder.GetId(), child.GetParentId());
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder.GetId(), child_name));
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder2.GetId(), child_name));
  child.PutParentId(parent_folder2.GetId());
  EXPECT_EQ(parent_folder2.GetId(), child.GetParentId());
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder.GetId(), child_name));
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder2.GetId(), child_name));
}

TEST_F(SyncableDirectoryTest, TestNoReindexDeletedItems) {
  std::string folder_name = "folder";
  std::string new_name = "new_name";

  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), folder_name);
  ASSERT_TRUE(folder.good());
  folder.PutIsDir(true);
  folder.PutIsDel(true);

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));

  MutableEntry deleted(&trans, GET_BY_ID, folder.GetId());
  ASSERT_TRUE(deleted.good());
  deleted.PutParentId(trans.root_id());
  deleted.PutNonUniqueName(new_name);

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));
  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), new_name));
}

TEST_F(SyncableDirectoryTest, TestCaseChangeRename) {
  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "CaseChange");
  ASSERT_TRUE(folder.good());
  folder.PutParentId(trans.root_id());
  folder.PutNonUniqueName("CASECHANGE");
  folder.PutIsDel(true);
}

// Create items of each model type, and check that GetModelType and
// GetServerModelType return the right value.
TEST_F(SyncableDirectoryTest, GetModelType) {
  TestIdFactory id_factory;
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType datatype : protocol_types) {
    SCOPED_TRACE(testing::Message("Testing model type ") << datatype);
    switch (datatype) {
      case UNSPECIFIED:
      case TOP_LEVEL_FOLDER:
        continue;  // Datatype isn't a function of Specifics.
      default:
        break;
    }
    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(datatype, &specifics);

    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "Folder");
    ASSERT_TRUE(folder.good());
    folder.PutId(id_factory.NewServerId());
    folder.PutSpecifics(specifics);
    folder.PutBaseVersion(1);
    folder.PutIsDir(true);
    folder.PutIsDel(false);
    ASSERT_EQ(datatype, folder.GetModelType());

    MutableEntry item(&trans, CREATE, BOOKMARKS, trans.root_id(), "Item");
    ASSERT_TRUE(item.good());
    item.PutId(id_factory.NewServerId());
    item.PutSpecifics(specifics);
    item.PutBaseVersion(1);
    item.PutIsDir(false);
    item.PutIsDel(false);
    ASSERT_EQ(datatype, item.GetModelType());

    // It's critical that deletion records retain their datatype, so that
    // they can be dispatched to the appropriate change processor.
    MutableEntry deleted_item(&trans, CREATE, BOOKMARKS, trans.root_id(),
                              "Deleted Item");
    ASSERT_TRUE(item.good());
    deleted_item.PutId(id_factory.NewServerId());
    deleted_item.PutSpecifics(specifics);
    deleted_item.PutBaseVersion(1);
    deleted_item.PutIsDir(false);
    deleted_item.PutIsDel(true);
    ASSERT_EQ(datatype, deleted_item.GetModelType());

    MutableEntry server_folder(&trans, CREATE_NEW_UPDATE_ITEM,
                               id_factory.NewServerId());
    ASSERT_TRUE(server_folder.good());
    server_folder.PutServerSpecifics(specifics);
    server_folder.PutBaseVersion(1);
    server_folder.PutServerIsDir(true);
    server_folder.PutServerIsDel(false);
    ASSERT_EQ(datatype, server_folder.GetServerModelType());

    MutableEntry server_item(&trans, CREATE_NEW_UPDATE_ITEM,
                             id_factory.NewServerId());
    ASSERT_TRUE(server_item.good());
    server_item.PutServerSpecifics(specifics);
    server_item.PutBaseVersion(1);
    server_item.PutServerIsDir(false);
    server_item.PutServerIsDel(false);
    ASSERT_EQ(datatype, server_item.GetServerModelType());

    sync_pb::SyncEntity folder_entity;
    folder_entity.set_id_string(SyncableIdToProto(id_factory.NewServerId()));
    folder_entity.set_deleted(false);
    folder_entity.set_folder(true);
    folder_entity.mutable_specifics()->CopyFrom(specifics);
    ASSERT_EQ(datatype, GetModelType(folder_entity));

    sync_pb::SyncEntity item_entity;
    item_entity.set_id_string(SyncableIdToProto(id_factory.NewServerId()));
    item_entity.set_deleted(false);
    item_entity.set_folder(false);
    item_entity.mutable_specifics()->CopyFrom(specifics);
    ASSERT_EQ(datatype, GetModelType(item_entity));
  }
}

// A test that roughly mimics the directory interaction that occurs when a
// bookmark folder and entry are created then synced for the first time.  It is
// a more common variant of the 'DeletedAndUnsyncedChild' scenario tested below.
TEST_F(SyncableDirectoryTest, ChangeEntryIDAndUpdateChildren_ParentAndChild) {
  TestIdFactory id_factory;
  Id orig_parent_id;
  Id orig_child_id;

  {
    // Create two client-side items, a parent and child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.GetId(), "child");
    child.PutIsUnsynced(true);

    orig_parent_id = parent.GetId();
    orig_child_id = child.GetId();
  }

  {
    // Simulate what happens after committing two items.  Their IDs will be
    // replaced with server IDs.  The child is renamed first, then the parent.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, GET_BY_ID, orig_parent_id);
    MutableEntry child(&trans, GET_BY_ID, orig_child_id);

    ChangeEntryIDAndUpdateChildren(&trans, &child, id_factory.NewServerId());
    child.PutIsUnsynced(false);
    child.PutBaseVersion(1);
    child.PutServerVersion(1);

    ChangeEntryIDAndUpdateChildren(&trans, &parent, id_factory.NewServerId());
    parent.PutIsUnsynced(false);
    parent.PutBaseVersion(1);
    parent.PutServerVersion(1);
  }

  // Final check for validity.
  EXPECT_EQ(OPENED_EXISTING, SimulateSaveAndReloadDir());
}

// A test that roughly mimics the directory interaction that occurs when a
// type root folder is created locally and then re-created (updated) from the
// server.
TEST_F(SyncableDirectoryTest, ChangeEntryIDAndUpdateChildren_ImplicitParent) {
  TestIdFactory id_factory;
  Id orig_parent_id;
  Id child_id;

  {
    // Create two client-side items, a parent and child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, PREFERENCES, id_factory.root(),
                        "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    // The child has unset parent ID. The parent is inferred from the type.
    MutableEntry child(&trans, CREATE, PREFERENCES, "child");
    child.PutIsUnsynced(true);

    orig_parent_id = parent.GetId();
    child_id = child.GetId();
  }

  {
    // Simulate what happens after committing two items.  Their IDs will be
    // replaced with server IDs.  The child is renamed first, then the parent.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, GET_BY_ID, orig_parent_id);

    ChangeEntryIDAndUpdateChildren(&trans, &parent, id_factory.NewServerId());
    parent.PutIsUnsynced(false);
    parent.PutBaseVersion(1);
    parent.PutServerVersion(1);
  }

  // Final check for validity.
  EXPECT_EQ(OPENED_EXISTING, SimulateSaveAndReloadDir());

  // Verify that child's PARENT_ID hasn't been updated.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry child(&trans, GET_BY_ID, child_id);
    EXPECT_TRUE(child.good());
    EXPECT_TRUE(child.GetParentId().IsNull());
  }
}

// A test based on the scenario where we create a bookmark folder and entry
// locally, but with a twist.  In this case, the bookmark is deleted before we
// are able to sync either it or its parent folder.  This scenario used to cause
// directory corruption, see crbug.com/125381.
TEST_F(SyncableDirectoryTest,
       ChangeEntryIDAndUpdateChildren_DeletedAndUnsyncedChild) {
  TestIdFactory id_factory;
  Id orig_parent_id;
  Id orig_child_id;

  {
    // Create two client-side items, a parent and child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.GetId(), "child");
    child.PutIsUnsynced(true);

    orig_parent_id = parent.GetId();
    orig_child_id = child.GetId();
  }

  {
    // Delete the child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry child(&trans, GET_BY_ID, orig_child_id);
    child.PutIsDel(true);
  }

  {
    // Simulate what happens after committing the parent.  Its ID will be
    // replaced with server a ID.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, GET_BY_ID, orig_parent_id);

    ChangeEntryIDAndUpdateChildren(&trans, &parent, id_factory.NewServerId());
    parent.PutIsUnsynced(false);
    parent.PutBaseVersion(1);
    parent.PutServerVersion(1);
  }

  // Final check for validity.
  EXPECT_EQ(OPENED_EXISTING, SimulateSaveAndReloadDir());
}

// Ask the directory to generate a unique ID.  Close and re-open the database
// without saving, then ask for another unique ID.  Verify IDs are not reused.
// This scenario simulates a crash within the first few seconds of operation.
TEST_F(SyncableDirectoryTest, LocalIdReuseTest) {
  Id pre_crash_id = dir()->NextId();
  SimulateCrashAndReloadDir();
  Id post_crash_id = dir()->NextId();
  EXPECT_NE(pre_crash_id, post_crash_id);
}

// Ask the directory to generate a unique ID.  Save the directory.  Close and
// re-open the database without saving, then ask for another unique ID.  Verify
// IDs are not reused.  This scenario simulates a steady-state crash.
TEST_F(SyncableDirectoryTest, LocalIdReuseTestWithSave) {
  Id pre_crash_id = dir()->NextId();
  dir()->SaveChanges();
  SimulateCrashAndReloadDir();
  Id post_crash_id = dir()->NextId();
  EXPECT_NE(pre_crash_id, post_crash_id);
}

// Ensure that the unsynced, is_del and server unkown entries that may have been
// left in the database by old clients will be deleted when we open the old
// database.
TEST_F(SyncableDirectoryTest, OldClientLeftUnsyncedDeletedLocalItem) {
  // We must create an entry with the offending properties.  This is done with
  // some abuse of the MutableEntry's API; it doesn't expect us to modify an
  // item after it is deleted.  If this hack becomes impractical we will need to
  // find a new way to simulate this scenario.

  TestIdFactory id_factory;

  // Happy-path: These valid entries should not get deleted.
  Id server_knows_id = id_factory.NewServerId();
  Id not_is_del_id = id_factory.NewLocalId();

  // The ID of the entry which will be unsynced, is_del and !ServerKnows().
  Id zombie_id = id_factory.NewLocalId();

  // We're about to do some bad things.  Tell the directory verification
  // routines to look the other way.
  dir()->SetInvariantCheckLevel(OFF);

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    // Create an uncommitted tombstone entry.
    MutableEntry server_knows(&trans, CREATE, BOOKMARKS, id_factory.root(),
                              "server_knows");
    server_knows.PutId(server_knows_id);
    server_knows.PutIsUnsynced(true);
    server_knows.PutIsDel(true);
    server_knows.PutBaseVersion(5);
    server_knows.PutServerVersion(4);

    // Create a valid update entry.
    MutableEntry not_is_del(&trans, CREATE, BOOKMARKS, id_factory.root(),
                            "not_is_del");
    not_is_del.PutId(not_is_del_id);
    not_is_del.PutIsDel(false);
    not_is_del.PutIsUnsynced(true);

    // Create a tombstone which should never be sent to the server because the
    // server never knew about the item's existence.
    //
    // New clients should never put entries into this state.  We work around
    // this by setting IS_DEL before setting IS_UNSYNCED, something which the
    // client should never do in practice.
    MutableEntry zombie(&trans, CREATE, BOOKMARKS, id_factory.root(), "zombie");
    zombie.PutId(zombie_id);
    zombie.PutIsDel(true);
    zombie.PutIsUnsynced(true);
  }

  ASSERT_EQ(OPENED_EXISTING, SimulateSaveAndReloadDir());

  {
    ReadTransaction trans(FROM_HERE, dir().get());

    // The directory loading routines should have cleaned things up, making it
    // safe to check invariants once again.
    dir()->FullyCheckTreeInvariants(&trans);

    Entry server_knows(&trans, GET_BY_ID, server_knows_id);
    EXPECT_TRUE(server_knows.good());

    Entry not_is_del(&trans, GET_BY_ID, not_is_del_id);
    EXPECT_TRUE(not_is_del.good());

    Entry zombie(&trans, GET_BY_ID, zombie_id);
    EXPECT_FALSE(zombie.good());
  }
}

TEST_F(SyncableDirectoryTest, PositionWithNullSurvivesSaveAndReload) {
  TestIdFactory id_factory;
  Id null_child_id;
  const char null_cstr[] = "\0null\0test";
  std::string null_str(null_cstr, base::size(null_cstr) - 1);
  // Pad up to the minimum length with 0x7f characters, then add a string that
  // contains a few nulls to the end.  This is slightly wrong, since the suffix
  // part of a UniquePosition shouldn't contain nulls, but it's good enough for
  // this test.
  std::string suffix =
      std::string(UniquePosition::kSuffixLength - null_str.length(), '\x7f') +
      null_str;
  UniquePosition null_pos = UniquePosition::FromInt64(10, suffix);

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.GetId(), "child");
    child.PutIsUnsynced(true);
    child.PutUniquePosition(null_pos);
    child.PutServerUniquePosition(null_pos);

    null_child_id = child.GetId();
  }

  EXPECT_EQ(OPENED_EXISTING, SimulateSaveAndReloadDir());

  {
    ReadTransaction trans(FROM_HERE, dir().get());

    Entry null_ordinal_child(&trans, GET_BY_ID, null_child_id);
    EXPECT_TRUE(null_pos.Equals(null_ordinal_child.GetUniquePosition()));
    EXPECT_TRUE(null_pos.Equals(null_ordinal_child.GetServerUniquePosition()));
  }
}

// Any item with BOOKMARKS in their local specifics should have a valid local
// unique position.  If there is an item in the loaded DB that does not match
// this criteria, we consider the whole DB to be corrupt.
TEST_F(SyncableDirectoryTest, BadPositionCountsAsCorruption) {
  TestIdFactory id_factory;

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    // The code is littered with DCHECKs that try to stop us from doing what
    // we're about to do.  Our work-around is to create a bookmark based on
    // a server update, then update its local specifics without updating its
    // local unique position.

    MutableEntry child(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.MakeServer("child"));
    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(BOOKMARKS, &specifics);
    child.PutIsUnappliedUpdate(true);
    child.PutSpecifics(specifics);

    EXPECT_TRUE(child.ShouldMaintainPosition());
    EXPECT_TRUE(!child.GetUniquePosition().IsValid());
  }

  EXPECT_EQ(FAILED_DATABASE_CORRUPT, SimulateSaveAndReloadDir());
}

TEST_F(SyncableDirectoryTest, General) {
  int64_t written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  // Test simple read operations on an empty DB.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    Directory::Metahandles child_handles;
    dir()->GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_TRUE(child_handles.empty());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    written_metahandle = me.GetMetahandle();
  }

  // Test GetChildHandles* after something is now in the DB.
  // Also check that GET_BY_ID works.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Directory::Metahandles child_handles;
    dir()->GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_EQ(1u, child_handles.size());

    for (auto i = child_handles.begin(); i != child_handles.end(); ++i) {
      EXPECT_EQ(*i, written_metahandle);
    }
  }

  // Test writing data to an entity. Also check that GET_BY_HANDLE works.
  static const char s[] = "Hello World.";
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    PutDataAsBookmarkFavicon(&trans, &e, s, sizeof(s));
  }

  // Test reading back the contents that we just wrote.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    ExpectDataFromBookmarkFaviconEquals(&trans, &e, s, sizeof(s));
  }

  // Verify it exists in the folder.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    EXPECT_EQ(1, CountEntriesWithName(&rtrans, rtrans.root_id(), name));
  }

  // Now delete it.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    e.PutIsDel(true);

    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), name));
  }

  dir()->SaveChanges();
}

TEST_F(SyncableDirectoryTest, ChildrenOps) {
  int64_t written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_FALSE(dir()->HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_TRUE(root.GetFirstChildId().IsNull());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    written_metahandle = me.GetMetahandle();
  }

  // Test children ops after something is now in the DB.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry child(&rtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(child.good());

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_TRUE(dir()->HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_EQ(e.GetId(), root.GetFirstChildId());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(me.good());
    me.PutIsDel(true);
  }

  // Test children ops after the children have been deleted.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_FALSE(dir()->HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_TRUE(root.GetFirstChildId().IsNull());
  }

  dir()->SaveChanges();
}

TEST_F(SyncableDirectoryTest, ClientIndexRebuildsProperly) {
  int64_t written_metahandle;
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string name = "cheesepuffs";
  std::string tag = "dietcoke";

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    me.PutUniqueClientTag(tag);
    written_metahandle = me.GetMetahandle();
  }
  dir()->SaveChanges();

  // Close and reopen, causing index regeneration.
  ReopenDirectory();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetBaseVersion(), 1);
    EXPECT_EQ(me.GetUniqueClientTag(), tag);
    EXPECT_EQ(me.GetMetahandle(), written_metahandle);
  }
}

TEST_F(SyncableDirectoryTest, ClientIndexRebuildsDeletedProperly) {
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string tag = "dietcoke";

  // Test creating a deleted, unsynced, server meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "deleted");
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    me.PutUniqueClientTag(tag);
    me.PutIsDel(true);
    me.PutIsUnsynced(true);  // Or it might be purged.
  }
  dir()->SaveChanges();

  // Close and reopen, causing index regeneration.
  ReopenDirectory();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    // Should still be present and valid in the client tag index.
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetUniqueClientTag(), tag);
    EXPECT_TRUE(me.GetIsDel());
    EXPECT_TRUE(me.GetIsUnsynced());
  }
}

TEST_F(SyncableDirectoryTest, ToValue) {
  const Id id = TestIdFactory::FromNumber(99);
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    EXPECT_FALSE(e.good());  // Hasn't been written yet.

    std::unique_ptr<base::DictionaryValue> value(e.ToValue(nullptr));
    ExpectDictBooleanValue(false, *value, "good");
    EXPECT_EQ(1u, value->size());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "new");
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);

    std::unique_ptr<base::DictionaryValue> value(me.ToValue(nullptr));
    ExpectDictBooleanValue(true, *value, "good");
    EXPECT_TRUE(value->HasKey("kernel"));
    ExpectDictStringValue("Bookmarks", *value, "modelType");
    ExpectDictBooleanValue(true, *value, "existsOnClientBecauseNameIsNonEmpty");
    ExpectDictBooleanValue(false, *value, "isRoot");
  }

  dir()->SaveChanges();
}

// A thread that creates a bunch of directory entries.
class StressTransactionsDelegate : public base::PlatformThread::Delegate {
 public:
  StressTransactionsDelegate(Directory* dir, int thread_number)
      : dir_(dir), thread_number_(thread_number) {}

 private:
  Directory* const dir_;
  const int thread_number_;

  // PlatformThread::Delegate methods:
  void ThreadMain() override {
    int entry_count = 0;
    std::string path_name;

    for (int i = 0; i < 20; ++i) {
      const int rand_action = base::RandInt(0, 9);
      if (rand_action < 4 && !path_name.empty()) {
        ReadTransaction trans(FROM_HERE, dir_);
        EXPECT_EQ(1, CountEntriesWithName(&trans, trans.root_id(), path_name));
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(base::RandInt(0, 9)));
      } else {
        std::string unique_name =
            base::StringPrintf("%d.%d", thread_number_, entry_count++);
        path_name.assign(unique_name.begin(), unique_name.end());
        WriteTransaction trans(FROM_HERE, UNITTEST, dir_);
        MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), path_name);
        EXPECT_TRUE(e.good());
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(base::RandInt(0, 19)));
        e.PutIsUnsynced(true);
        if (e.PutId(TestIdFactory::FromNumber(base::RandInt(0, RAND_MAX))) &&
            e.GetId().ServerKnows() && !e.GetId().IsRoot()) {
          e.PutBaseVersion(1);
        }
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(StressTransactionsDelegate);
};

// Stress test Directory by accessing it from several threads concurrently.
TEST_F(SyncableDirectoryTest, StressTransactions) {
  const int kThreadCount = 7;
  base::PlatformThreadHandle threads[kThreadCount];
  std::unique_ptr<StressTransactionsDelegate> thread_delegates[kThreadCount];

  for (int i = 0; i < kThreadCount; ++i) {
    thread_delegates[i] =
        std::make_unique<StressTransactionsDelegate>(dir().get(), i);
    ASSERT_TRUE(base::PlatformThread::Create(0, thread_delegates[i].get(),
                                             &threads[i]));
  }

  for (int i = 0; i < kThreadCount; ++i) {
    base::PlatformThread::Join(threads[i]);
  }
}

// Verify that the directory accepts entries with unset parent ID.
TEST_F(SyncableDirectoryTest, MutableEntry_ImplicitParentId) {
  TestIdFactory id_factory;
  const Id root_id = TestIdFactory::root();
  const Id p_root_id = id_factory.NewServerId();
  const Id a_root_id = id_factory.NewServerId();
  const Id item1_id = id_factory.NewServerId();
  const Id item2_id = id_factory.NewServerId();
  const Id item3_id = id_factory.NewServerId();
  // Create two type root folders that are necessary (for now)
  // for creating items without explicitly set Parent ID
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry p_root(&trans, CREATE, PREFERENCES, root_id, "P");
    ASSERT_TRUE(p_root.good());
    p_root.PutIsDir(true);
    p_root.PutId(p_root_id);
    p_root.PutBaseVersion(1);

    MutableEntry a_root(&trans, CREATE, AUTOFILL, root_id, "A");
    ASSERT_TRUE(a_root.good());
    a_root.PutIsDir(true);
    a_root.PutId(a_root_id);
    a_root.PutBaseVersion(1);
  }

  // Create two entries with implicit parent nodes and one entry with explicit
  // parent node.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry item1(&trans, CREATE, PREFERENCES, "P1");
    item1.PutBaseVersion(1);
    item1.PutId(item1_id);
    MutableEntry item2(&trans, CREATE, AUTOFILL, "A1");
    item2.PutBaseVersion(1);
    item2.PutId(item2_id);
    // Placing an AUTOFILL item under the root isn't expected,
    // but let's test it to verify that explicit root overrides the implicit
    // one and this entry doesn't end up under the "A" root.
    MutableEntry item3(&trans, CREATE, AUTOFILL, root_id, "A2");
    item3.PutBaseVersion(1);
    item3.PutId(item3_id);
  }

  {
    ReadTransaction trans(FROM_HERE, dir().get());
    // Verify that item1 and item2 are good and have no ParentId.
    Entry item1(&trans, GET_BY_ID, item1_id);
    ASSERT_TRUE(item1.good());
    ASSERT_TRUE(item1.GetParentId().IsNull());
    Entry item2(&trans, GET_BY_ID, item2_id);
    ASSERT_TRUE(item2.good());
    ASSERT_TRUE(item2.GetParentId().IsNull());
    // Verify that p_root and a_root have exactly one child each
    // (subtract one to exclude roots themselves).
    Entry p_root(&trans, GET_BY_ID, p_root_id);
    ASSERT_EQ(item1_id, p_root.GetFirstChildId());
    ASSERT_EQ(1, p_root.GetTotalNodeCount() - 1);
    Entry a_root(&trans, GET_BY_ID, a_root_id);
    ASSERT_EQ(item2_id, a_root.GetFirstChildId());
    ASSERT_EQ(1, a_root.GetTotalNodeCount() - 1);
  }
}

// Verify that the successor / predecessor navigation still works for
// directory entries with unset Parent IDs.
TEST_F(SyncableDirectoryTest, MutableEntry_ImplicitParentId_Siblings) {
  TestIdFactory id_factory;
  const Id root_id = TestIdFactory::root();
  const Id p_root_id = id_factory.NewServerId();
  const Id item1_id = id_factory.FromNumber(1);
  const Id item2_id = id_factory.FromNumber(2);

  // Create type root folder for PREFERENCES.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry p_root(&trans, CREATE, PREFERENCES, root_id, "P");
    ASSERT_TRUE(p_root.good());
    p_root.PutIsDir(true);
    p_root.PutId(p_root_id);
    p_root.PutBaseVersion(1);
  }

  // Create two PREFERENCES entries with implicit parent nodes.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry item1(&trans, CREATE, PREFERENCES, "P1");
    item1.PutBaseVersion(1);
    item1.PutId(item1_id);
    MutableEntry item2(&trans, CREATE, PREFERENCES, "P2");
    item2.PutBaseVersion(1);
    item2.PutId(item2_id);
  }

  // Verify GetSuccessorId and GetPredecessorId calls for these items.
  // Please note that items are sorted according to their ID, e.g.
  // item1 first, then item2.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry item1(&trans, GET_BY_ID, item1_id);
    EXPECT_EQ(Id(), item1.GetPredecessorId());
    EXPECT_EQ(item2_id, item1.GetSuccessorId());

    Entry item2(&trans, GET_BY_ID, item2_id);
    EXPECT_EQ(item1_id, item2.GetPredecessorId());
    EXPECT_EQ(Id(), item2.GetSuccessorId());
  }
}

TEST_F(SyncableDirectoryTest, SaveChangesSnapshot_HasUnsavedMetahandleChanges) {
  Directory::SaveChangesSnapshot snapshot;
  EXPECT_FALSE(snapshot.HasUnsavedMetahandleChanges());
  snapshot.dirty_metas.insert(std::make_unique<EntryKernel>());
  EXPECT_TRUE(snapshot.HasUnsavedMetahandleChanges());
  snapshot.dirty_metas.clear();

  EXPECT_FALSE(snapshot.HasUnsavedMetahandleChanges());
  snapshot.metahandles_to_purge.insert(1);
  EXPECT_TRUE(snapshot.HasUnsavedMetahandleChanges());
  snapshot.metahandles_to_purge.clear();
}

// Verify that Directory triggers an unrecoverable error when a catastrophic
// DirectoryBackingStore error is detected.
TEST_F(SyncableDirectoryTest, CatastrophicError) {
  MockUnrecoverableErrorHandler unrecoverable_error_handler;
  Directory dir(
      std::make_unique<InMemoryDirectoryBackingStore>(
          "catastrophic_error", base::BindRepeating([]() -> std::string {
            return "test_cache_guid";
          })),
      MakeWeakHandle(unrecoverable_error_handler.GetWeakPtr()),
      base::RepeatingClosure(), nullptr);
  ASSERT_EQ(OPENED_NEW, dir.Open(kDirectoryName, directory_change_delegate(),
                                 NullTransactionObserver()));
  ASSERT_EQ(0, unrecoverable_error_handler.invocation_count());

  // Fire off two catastrophic errors. Call it twice to ensure Directory is
  // tolerant of multiple invocations since that may happen in the real world.
  dir.OnCatastrophicError();
  dir.OnCatastrophicError();

  base::RunLoop().RunUntilIdle();

  // See that the unrecoverable error handler has been invoked twice.
  ASSERT_EQ(2, unrecoverable_error_handler.invocation_count());
}

bool EntitySpecificsValuesAreSame(const sync_pb::EntitySpecifics& v1,
                                  const sync_pb::EntitySpecifics& v2) {
  return &v1 == &v2;
}

// Verifies that server and client specifics are shared when their values
// are equal.
TEST_F(SyncableDirectoryTest, SharingOfClientAndServerSpecifics) {
  sync_pb::EntitySpecifics specifics1;
  sync_pb::EntitySpecifics specifics2;
  sync_pb::EntitySpecifics specifics3;
  AddDefaultFieldValue(BOOKMARKS, &specifics1);
  AddDefaultFieldValue(BOOKMARKS, &specifics2);
  AddDefaultFieldValue(BOOKMARKS, &specifics3);
  specifics1.mutable_bookmark()->set_url("foo");
  specifics2.mutable_bookmark()->set_url("bar");
  // specifics3 has the same URL as specifics1
  specifics3.mutable_bookmark()->set_url("foo");

  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry item(&trans, CREATE, BOOKMARKS, trans.root_id(), "item");
  item.PutId(TestIdFactory::FromNumber(1));
  item.PutBaseVersion(10);

  // Verify sharing.
  item.PutSpecifics(specifics1);
  item.PutServerSpecifics(specifics1);
  EXPECT_TRUE(EntitySpecificsValuesAreSame(item.GetSpecifics(),
                                           item.GetServerSpecifics()));

  // Verify that specifics are no longer shared.
  item.PutServerSpecifics(specifics2);
  EXPECT_FALSE(EntitySpecificsValuesAreSame(item.GetSpecifics(),
                                            item.GetServerSpecifics()));

  // Verify that specifics are shared again because specifics3 matches
  // specifics1.
  item.PutServerSpecifics(specifics3);
  EXPECT_TRUE(EntitySpecificsValuesAreSame(item.GetSpecifics(),
                                           item.GetServerSpecifics()));

  // Verify that copying the same value back to SPECIFICS is still OK.
  item.PutSpecifics(specifics3);
  EXPECT_TRUE(EntitySpecificsValuesAreSame(item.GetSpecifics(),
                                           item.GetServerSpecifics()));

  // Verify sharing with BASE_SERVER_SPECIFICS.
  EXPECT_FALSE(EntitySpecificsValuesAreSame(item.GetServerSpecifics(),
                                            item.GetBaseServerSpecifics()));
  item.PutBaseServerSpecifics(specifics3);
  EXPECT_TRUE(EntitySpecificsValuesAreSame(item.GetServerSpecifics(),
                                           item.GetBaseServerSpecifics()));
}

// Tests checking and marking a type as having its initial sync completed.
TEST_F(SyncableDirectoryTest, InitialSyncEndedForType) {
  // Not completed if there is no root node.
  EXPECT_FALSE(dir()->InitialSyncEndedForType(PREFERENCES));

  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  // Create the root node.
  ModelNeutralMutableEntry entry(&trans, syncable::CREATE_NEW_TYPE_ROOT,
                                 PREFERENCES);
  ASSERT_TRUE(entry.good());

  entry.PutServerIsDir(true);
  entry.PutUniqueServerTag(ModelTypeToRootTag(PREFERENCES));

  // Should still be marked as incomplete.
  EXPECT_FALSE(dir()->InitialSyncEndedForType(&trans, PREFERENCES));

  // Mark as complete and verify.
  dir()->MarkInitialSyncEndedForType(&trans, PREFERENCES);
  EXPECT_TRUE(dir()->InitialSyncEndedForType(&trans, PREFERENCES));
}

TEST_F(SyncableDirectoryTest, TestGetNodeDetailsForType) {
  CreateEntry(BOOKMARKS, "rtc");

  ReadTransaction trans(FROM_HERE, dir().get());
  std::unique_ptr<base::ListValue> nodes(
      dir()->GetNodeDetailsForType(&trans, BOOKMARKS));
  ASSERT_EQ(1U, nodes->GetSize());

  const base::DictionaryValue* first_result;
  ASSERT_TRUE(nodes->GetDictionary(0, &first_result));
  EXPECT_TRUE(first_result->HasKey("ID"));
  EXPECT_TRUE(first_result->HasKey("NON_UNIQUE_NAME"));
}

}  // namespace syncable

}  // namespace syncer
