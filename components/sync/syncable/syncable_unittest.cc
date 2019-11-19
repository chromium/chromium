// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/sync/base/fake_encryptor.h"
#include "components/sync/base/test_unrecoverable_error_handler.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/syncable/directory_backing_store.h"
#include "components/sync/syncable/directory_change_delegate.h"
#include "components/sync/syncable/directory_unittest.h"
#include "components/sync/syncable/in_memory_directory_backing_store.h"
#include "components/sync/syncable/metahandle_set.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/on_disk_directory_backing_store.h"
#include "components/sync/syncable/syncable_proto_util.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "components/sync/test/engine/test_syncable_utils.h"
#include "components/sync/test/null_directory_change_delegate.h"
#include "components/sync/test/null_transaction_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {

using base::ExpectDictBooleanValue;
using base::ExpectDictStringValue;

// An OnDiskDirectoryBackingStore that can be set to always fail SaveChanges.
class TestBackingStore : public OnDiskDirectoryBackingStore {
 public:
  TestBackingStore(const std::string& dir_name,
                   const base::FilePath& backing_filepath);

  ~TestBackingStore() override;

  bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot) override;

  void StartFailingSaveChanges() { fail_save_changes_ = true; }

 private:
  bool fail_save_changes_;
};

TestBackingStore::TestBackingStore(const std::string& dir_name,
                                   const base::FilePath& backing_filepath)
    : OnDiskDirectoryBackingStore(dir_name,
                                  base::BindRepeating([]() -> std::string {
                                    return "test_cache_guid";
                                  }),
                                  backing_filepath),
      fail_save_changes_(false) {}

TestBackingStore::~TestBackingStore() {}

bool TestBackingStore::SaveChanges(
    const Directory::SaveChangesSnapshot& snapshot) {
  if (fail_save_changes_) {
    return false;
  } else {
    return OnDiskDirectoryBackingStore::SaveChanges(snapshot);
  }
}

// A directory whose Save() function can be set to always fail.
class TestDirectory : public Directory {
 public:
  // A factory function used to work around some initialization order issues.
  static std::unique_ptr<TestDirectory> Create(
      Encryptor* encryptor,
      const WeakHandle<UnrecoverableErrorHandler>& handler,
      const std::string& dir_name,
      const base::FilePath& backing_filepath);

  ~TestDirectory() override;

  void StartFailingSaveChanges() { backing_store_->StartFailingSaveChanges(); }

 private:
  TestDirectory(Encryptor* encryptor,
                const WeakHandle<UnrecoverableErrorHandler>& handler,
                TestBackingStore* backing_store);

  TestBackingStore* backing_store_;
};

std::unique_ptr<TestDirectory> TestDirectory::Create(
    Encryptor* encryptor,
    const WeakHandle<UnrecoverableErrorHandler>& handler,
    const std::string& dir_name,
    const base::FilePath& backing_filepath) {
  TestBackingStore* backing_store =
      new TestBackingStore(dir_name, backing_filepath);
  return base::WrapUnique(new TestDirectory(encryptor, handler, backing_store));
}

TestDirectory::TestDirectory(
    Encryptor* encryptor,
    const WeakHandle<UnrecoverableErrorHandler>& handler,
    TestBackingStore* backing_store)
    : Directory(base::WrapUnique(backing_store),
                handler,
                base::Closure(),
                nullptr),
      backing_store_(backing_store) {}

TestDirectory::~TestDirectory() {}

// crbug.com/144422
#if defined(OS_ANDROID)
#define MAYBE_FailInitialWrite DISABLED_FailInitialWrite
#else
#define MAYBE_FailInitialWrite FailInitialWrite
#endif
TEST(OnDiskSyncableDirectory, MAYBE_FailInitialWrite) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeEncryptor encryptor;
  TestUnrecoverableErrorHandler handler;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("Test.sqlite3"));
  std::string name = "user@x.com";
  NullDirectoryChangeDelegate delegate;

  std::unique_ptr<TestDirectory> test_dir = TestDirectory::Create(
      &encryptor, MakeWeakHandle(handler.GetWeakPtr()), name, file_path);

  test_dir->StartFailingSaveChanges();
  ASSERT_EQ(FAILED_INITIAL_WRITE,
            test_dir->Open(name, &delegate, NullTransactionObserver()));
}

// A variant of SyncableDirectoryTest that uses a real sqlite database.
class OnDiskSyncableDirectoryTest : public SyncableDirectoryTest {
 protected:
  // SetUp() is called before each test case is run.
  // The sqlite3 DB is deleted before each test is run.
  void SetUp() override {
    SyncableDirectoryTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Test.sqlite3"));
    base::DeleteFile(file_path_, false);
    ASSERT_EQ(OPENED_NEW, CreateDirectory());
    ASSERT_TRUE(dir()->good());
  }

  void TearDown() override {
    // This also closes file handles.
    dir()->SaveChanges();
    dir().reset();
    base::DeleteFile(file_path_, false);
    SyncableDirectoryTest::TearDown();
  }

  // Creates a new directory.  Deletes the old directory, if it exists.
  DirOpenResult CreateDirectory() {
    std::unique_ptr<TestDirectory> test_directory = TestDirectory::Create(
        encryptor(),
        MakeWeakHandle(unrecoverable_error_handler()->GetWeakPtr()),
        kDirectoryName, file_path_);
    test_directory_ = test_directory.get();
    dir() = std::move(test_directory);
    DCHECK(dir());
    DirOpenResult result = dir()->Open(
        kDirectoryName, directory_change_delegate(), NullTransactionObserver());
    dir()->set_cache_guid(dir()->legacy_cache_guid());
    return result;
  }

  void SaveAndReloadDir() {
    dir()->SaveChanges();
    ASSERT_EQ(OPENED_EXISTING, CreateDirectory());
    ASSERT_TRUE(dir()->good());
  }

  void StartFailingSaveChanges() { test_directory_->StartFailingSaveChanges(); }

  TestDirectory* test_directory_;  // mirrors std::unique_ptr<Directory> dir_
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
};

sync_pb::DataTypeContext BuildContext(ModelType type) {
  sync_pb::DataTypeContext context;
  context.set_context("context");
  context.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
  return context;
}

TEST_F(OnDiskSyncableDirectoryTest, TestPurgeEntriesWithTypeIn) {
  sync_pb::EntitySpecifics bookmark_specs;
  sync_pb::EntitySpecifics autofill_specs;
  sync_pb::EntitySpecifics preference_specs;
  AddDefaultFieldValue(BOOKMARKS, &bookmark_specs);
  AddDefaultFieldValue(PREFERENCES, &preference_specs);
  AddDefaultFieldValue(AUTOFILL, &autofill_specs);

  ModelTypeSet types_to_purge(PREFERENCES, AUTOFILL);

  dir()->SetDownloadProgress(BOOKMARKS, BuildProgress(BOOKMARKS));
  dir()->SetDownloadProgress(PREFERENCES, BuildProgress(PREFERENCES));
  dir()->SetDownloadProgress(AUTOFILL, BuildProgress(AUTOFILL));

  TestIdFactory id_factory;
  // Create some items for each type.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->SetDataTypeContext(&trans, BOOKMARKS, BuildContext(BOOKMARKS));
    dir()->SetDataTypeContext(&trans, PREFERENCES, BuildContext(PREFERENCES));
    dir()->SetDataTypeContext(&trans, AUTOFILL, BuildContext(AUTOFILL));

    // Make it look like these types have completed initial sync.
    CreateTypeRoot(&trans, dir().get(), BOOKMARKS);
    CreateTypeRoot(&trans, dir().get(), PREFERENCES);
    CreateTypeRoot(&trans, dir().get(), AUTOFILL);

    // Add more nodes for this type.  Technically, they should be placed under
    // the proper type root nodes but the assertions in this test won't notice
    // if their parent isn't quite right.
    MutableEntry item1(&trans, CREATE, BOOKMARKS, trans.root_id(), "Item");
    ASSERT_TRUE(item1.good());
    item1.PutServerSpecifics(bookmark_specs);
    item1.PutIsUnsynced(true);

    MutableEntry item2(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item2.good());
    item2.PutServerSpecifics(bookmark_specs);
    item2.PutIsUnappliedUpdate(true);

    MutableEntry item3(&trans, CREATE, PREFERENCES, trans.root_id(), "Item");
    ASSERT_TRUE(item3.good());
    item3.PutSpecifics(preference_specs);
    item3.PutServerSpecifics(preference_specs);
    item3.PutIsUnsynced(true);

    MutableEntry item4(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item4.good());
    item4.PutServerSpecifics(preference_specs);
    item4.PutIsUnappliedUpdate(true);

    MutableEntry item5(&trans, CREATE, AUTOFILL, trans.root_id(), "Item");
    ASSERT_TRUE(item5.good());
    item5.PutSpecifics(autofill_specs);
    item5.PutServerSpecifics(autofill_specs);
    item5.PutIsUnsynced(true);

    MutableEntry item6(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item6.good());
    item6.PutServerSpecifics(autofill_specs);
    item6.PutIsUnappliedUpdate(true);
  }

  dir()->SaveChanges();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    MetahandleSet all_set;
    GetAllMetaHandles(&trans, &all_set);
    ASSERT_EQ(10U, all_set.size());
  }

  dir()->PurgeEntriesWithTypeIn(types_to_purge, ModelTypeSet(), ModelTypeSet());

  // We first query the in-memory data, and then reload the directory (without
  // saving) to verify that disk does not still have the data.
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, true);
  SaveAndReloadDir();
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, false);
}

TEST_F(OnDiskSyncableDirectoryTest,
       TestSimpleFieldsPreservedDuringSaveChanges) {
  Id update_id = TestIdFactory::FromNumber(1);
  Id create_id;
  EntryKernel create_pre_save, update_pre_save;
  EntryKernel create_post_save, update_post_save;
  std::string create_name = "Create";

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry create(&trans, CREATE, BOOKMARKS, trans.root_id(),
                        create_name);
    MutableEntry update(&trans, CREATE_NEW_UPDATE_ITEM, update_id);
    create.PutIsUnsynced(true);
    update.PutIsUnappliedUpdate(true);
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_favicon("PNG");
    specifics.mutable_bookmark()->set_url("http://nowhere");
    create.PutSpecifics(specifics);
    update.PutServerSpecifics(specifics);
    create_pre_save = create.GetKernelCopy();
    update_pre_save = update.GetKernelCopy();
    create_id = create.GetId();
  }

  dir()->SaveChanges();
  dir() = std::make_unique<Directory>(
      std::make_unique<OnDiskDirectoryBackingStore>(
          kDirectoryName, base::BindRepeating([]() -> std::string {
            return "test_cache_guid";
          }),
          file_path_),
      MakeWeakHandle(unrecoverable_error_handler()->GetWeakPtr()),
      base::Closure(), nullptr);

  ASSERT_TRUE(dir().get());
  ASSERT_EQ(OPENED_EXISTING,
            dir()->Open(kDirectoryName, directory_change_delegate(),
                        NullTransactionObserver()));
  ASSERT_TRUE(dir()->good());

  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry create(&trans, GET_BY_ID, create_id);
    EXPECT_EQ(1, CountEntriesWithName(&trans, trans.root_id(), create_name));
    Entry update(&trans, GET_BY_ID, update_id);
    create_post_save = create.GetKernelCopy();
    update_post_save = update.GetKernelCopy();
  }
  int i = BEGIN_FIELDS;
  for (; i < INT64_FIELDS_END; ++i) {
    EXPECT_EQ(
        create_pre_save.ref((Int64Field)i) + (i == TRANSACTION_VERSION ? 1 : 0),
        create_post_save.ref((Int64Field)i))
        << "int64_t field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((Int64Field)i),
              update_post_save.ref((Int64Field)i))
        << "int64_t field #" << i << " changed during save/load";
  }
  for (; i < TIME_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((TimeField)i),
              create_post_save.ref((TimeField)i))
        << "time field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((TimeField)i),
              update_post_save.ref((TimeField)i))
        << "time field #" << i << " changed during save/load";
  }
  for (; i < ID_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((IdField)i), create_post_save.ref((IdField)i))
        << "id field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((IdField)i), update_pre_save.ref((IdField)i))
        << "id field #" << i << " changed during save/load";
  }
  for (; i < BIT_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((BitField)i),
              create_post_save.ref((BitField)i))
        << "Bit field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((BitField)i),
              update_post_save.ref((BitField)i))
        << "Bit field #" << i << " changed during save/load";
  }
  for (; i < STRING_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((StringField)i),
              create_post_save.ref((StringField)i))
        << "String field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((StringField)i),
              update_post_save.ref((StringField)i))
        << "String field #" << i << " changed during save/load";
  }
  for (; i < PROTO_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((ProtoField)i).SerializeAsString(),
              create_post_save.ref((ProtoField)i).SerializeAsString())
        << "Blob field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((ProtoField)i).SerializeAsString(),
              update_post_save.ref((ProtoField)i).SerializeAsString())
        << "Blob field #" << i << " changed during save/load";
  }
  for (; i < UNIQUE_POSITION_FIELDS_END; ++i) {
    EXPECT_TRUE(create_pre_save.ref((UniquePositionField)i)
                    .Equals(create_post_save.ref((UniquePositionField)i)))
        << "Position field #" << i << " changed during save/load";
    EXPECT_TRUE(update_pre_save.ref((UniquePositionField)i)
                    .Equals(update_post_save.ref((UniquePositionField)i)))
        << "Position field #" << i << " changed during save/load";
  }
}

TEST_F(OnDiskSyncableDirectoryTest, TestSaveChangesFailure) {
  int64_t handle1 = 0;
  // Set up an item using a regular, saveable directory.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.GetMetahandle();
    e1.PutBaseVersion(1);
    e1.PutIsDir(true);
    e1.PutId(TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir()->SaveChanges());

  // Make sure the item is no longer dirty after saving,
  // and make a modification.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.GetNonUniqueName(), "aguilera");
    aguilera.PutNonUniqueName("overwritten");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir()->SaveChanges());

  // Now do some operations when SaveChanges() will fail.
  StartFailingSaveChanges();
  ASSERT_TRUE(dir()->good());

  int64_t handle2 = 0;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.GetNonUniqueName(), "overwritten");
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_FALSE(IsInDirtyMetahandles(handle1));
    aguilera.PutNonUniqueName("christina");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));

    // New item.
    MutableEntry kids_on_block(&trans, CREATE, BOOKMARKS, trans.root_id(),
                               "kids");
    ASSERT_TRUE(kids_on_block.good());
    handle2 = kids_on_block.GetMetahandle();
    kids_on_block.PutBaseVersion(1);
    kids_on_block.PutIsDir(true);
    kids_on_block.PutId(TestIdFactory::FromNumber(102));
    EXPECT_TRUE(kids_on_block.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle2));
  }

  // We are using an unsaveable directory, so this can't succeed.  However,
  // the HandleSaveChangesFailure code path should have been triggered.
  ASSERT_FALSE(dir()->SaveChanges());

  // Make sure things were rolled back and the world is as it was before call.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry e1(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e1.good());
    EntryKernel aguilera = e1.GetKernelCopy();
    Entry kids(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(kids.good());
    EXPECT_TRUE(kids.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle2));
    EXPECT_TRUE(aguilera.is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
}

TEST_F(OnDiskSyncableDirectoryTest, TestSaveChangesFailureWithPurge) {
  int64_t handle1 = 0;
  // Set up an item and progress marker using a regular, saveable directory.
  dir()->SetDownloadProgress(BOOKMARKS, BuildProgress(BOOKMARKS));
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.GetMetahandle();
    e1.PutBaseVersion(1);
    e1.PutIsDir(true);
    e1.PutId(TestIdFactory::FromNumber(101));
    sync_pb::EntitySpecifics bookmark_specs;
    AddDefaultFieldValue(BOOKMARKS, &bookmark_specs);
    e1.PutSpecifics(bookmark_specs);
    e1.PutServerSpecifics(bookmark_specs);
    e1.PutId(TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir()->SaveChanges());

  // Now do some operations while SaveChanges() is set to fail.
  StartFailingSaveChanges();
  ASSERT_TRUE(dir()->good());

  ModelTypeSet set(BOOKMARKS);
  dir()->PurgeEntriesWithTypeIn(set, ModelTypeSet(), ModelTypeSet());
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
  ASSERT_FALSE(dir()->SaveChanges());
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
}

class SyncableDirectoryManagement : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  FakeEncryptor encryptor_;
  TestUnrecoverableErrorHandler handler_;
  NullDirectoryChangeDelegate delegate_;
};

TEST_F(SyncableDirectoryManagement, TestFileRelease) {
  base::FilePath path =
      temp_dir_.GetPath().Append(Directory::kSyncDatabaseFilename);

  {
    Directory dir(std::make_unique<OnDiskDirectoryBackingStore>(
                      "ScopeTest", base::BindRepeating([]() -> std::string {
                        return "test_cache_guid";
                      }),
                      path),
                  MakeWeakHandle(handler_.GetWeakPtr()), base::Closure(),
                  nullptr);
    DirOpenResult result =
        dir.Open("ScopeTest", &delegate_, NullTransactionObserver());
    ASSERT_EQ(result, OPENED_NEW);
  }

  // Destroying the directory should have released the backing database file.
  ASSERT_TRUE(base::DeleteFileRecursively(path));
}

class SyncableClientTagTest : public SyncableDirectoryTest {
 public:
  static const int kBaseVersion = 1;
  const char* test_name_;
  const char* test_tag_;

  SyncableClientTagTest() : test_name_("test_name"), test_tag_("dietcoke") {}

  bool CreateWithDefaultTag(Id id, bool deleted) {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, PREFERENCES, wtrans.root_id(), test_name_);
    EXPECT_TRUE(me.good());
    me.PutId(id);
    if (id.ServerKnows()) {
      me.PutBaseVersion(kBaseVersion);
    }
    me.PutIsUnsynced(true);
    me.PutIsDel(deleted);
    me.PutIsDir(false);
    return me.PutUniqueClientTag(test_tag_);
  }

  // Verify an entry exists with the default tag.
  void VerifyTag(Id id, bool deleted) {
    // Should still be present and valid in the client tag index.
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetUniqueClientTag(), test_tag_);
    EXPECT_EQ(me.GetIsDel(), deleted);

    // We only sync deleted items that the server knew about.
    if (me.GetId().ServerKnows() || !me.GetIsDel()) {
      EXPECT_EQ(me.GetIsUnsynced(), true);
    }
  }

 protected:
  TestIdFactory factory_;
};

TEST_F(SyncableClientTagTest, TestClientTagClear) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, false));
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_TRUE(me.good());
    me.PutUniqueClientTag(std::string());
  }
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry by_tag(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_FALSE(by_tag.good());

    Entry by_id(&trans, GET_BY_ID, server_id);
    EXPECT_TRUE(by_id.good());
    EXPECT_TRUE(by_id.GetUniqueClientTag().empty());
  }
}

TEST_F(SyncableClientTagTest, TestClientTagIndexServerId) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, false));
  VerifyTag(server_id, false);
}

TEST_F(SyncableClientTagTest, TestClientTagIndexClientId) {
  Id client_id = factory_.NewLocalId();
  EXPECT_TRUE(CreateWithDefaultTag(client_id, false));
  VerifyTag(client_id, false);
}

TEST_F(SyncableClientTagTest, TestDeletedClientTagIndexClientId) {
  Id client_id = factory_.NewLocalId();
  EXPECT_TRUE(CreateWithDefaultTag(client_id, true));
  VerifyTag(client_id, true);
}

TEST_F(SyncableClientTagTest, TestDeletedClientTagIndexServerId) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, true));
  VerifyTag(server_id, true);
}

TEST_F(SyncableClientTagTest, TestClientTagIndexDuplicateServer) {
  EXPECT_TRUE(CreateWithDefaultTag(factory_.NewServerId(), true));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewServerId(), true));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewServerId(), false));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewLocalId(), false));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewLocalId(), true));
}

}  // namespace syncable
}  // namespace syncer
