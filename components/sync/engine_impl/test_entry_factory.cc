// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/test_entry_factory.h"

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_id.h"
#include "components/sync/syncable/syncable_model_neutral_write_transaction.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_id_factory.h"

using std::string;

namespace syncer {

using syncable::Id;
using syncable::MutableEntry;
using syncable::UNITTEST;

TestEntryFactory::TestEntryFactory(syncable::Directory* dir)
    : directory_(dir), next_revision_(1) {}

TestEntryFactory::~TestEntryFactory() {}

int64_t TestEntryFactory::CreateUnappliedNewItemWithParent(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    const string& parent_id) {
  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                     Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.PutServerVersion(GetNextRevision());
  entry.PutIsUnappliedUpdate(true);

  entry.PutServerNonUniqueName(item_id);
  entry.PutServerParentId(Id::CreateFromServerId(parent_id));
  entry.PutServerIsDir(true);
  entry.PutServerSpecifics(specifics);
  return entry.GetMetahandle();
}

int64_t TestEntryFactory::CreateUnappliedNewBookmarkItemWithParent(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    const string& parent_id) {
  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                     Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.PutServerVersion(GetNextRevision());
  entry.PutIsUnappliedUpdate(true);

  entry.PutServerNonUniqueName(item_id);
  entry.PutServerParentId(Id::CreateFromServerId(parent_id));
  entry.PutServerIsDir(true);
  entry.PutServerSpecifics(specifics);

  return entry.GetMetahandle();
}

int64_t TestEntryFactory::CreateUnappliedNewItem(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    bool is_unique) {
  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                     Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.PutServerVersion(GetNextRevision());
  entry.PutIsUnappliedUpdate(true);
  entry.PutServerNonUniqueName(item_id);
  entry.PutServerParentId(syncable::Id::GetRoot());
  entry.PutServerIsDir(is_unique);
  entry.PutServerSpecifics(specifics);
  if (is_unique) {  // For top-level nodes.
    entry.PutUniqueServerTag(
        ModelTypeToRootTag(GetModelTypeFromSpecifics(specifics)));
  }
  return entry.GetMetahandle();
}

void TestEntryFactory::CreateUnsyncedItem(const Id& item_id,
                                          const Id& parent_id,
                                          const string& name,
                                          bool is_folder,
                                          ModelType model_type,
                                          int64_t* metahandle_out) {
  if (is_folder) {
    DCHECK_EQ(model_type, BOOKMARKS);
  }

  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);

  MutableEntry entry(&trans, syncable::CREATE, model_type, parent_id, name);
  DCHECK(entry.good());
  entry.PutId(item_id);
  entry.PutBaseVersion(item_id.ServerKnows() ? GetNextRevision() : 0);
  entry.PutIsUnsynced(true);
  entry.PutIsDir(is_folder);
  entry.PutIsDel(false);
  entry.PutParentId(parent_id);
  sync_pb::EntitySpecifics default_specifics;
  AddDefaultFieldValue(model_type, &default_specifics);
  entry.PutSpecifics(default_specifics);

  if (item_id.ServerKnows()) {
    entry.PutServerSpecifics(default_specifics);
    entry.PutServerIsDir(false);
    entry.PutServerParentId(parent_id);
    entry.PutServerIsDel(false);
  }
  if (metahandle_out)
    *metahandle_out = entry.GetMetahandle();
}

int64_t TestEntryFactory::CreateUnappliedAndUnsyncedBookmarkItem(
    const string& name) {
  int64_t metahandle = 0;
  CreateUnsyncedItem(TestIdFactory::MakeServer(name), TestIdFactory::root(),
                     name, false, BOOKMARKS, &metahandle);

  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, metahandle);
  if (!entry.good()) {
    NOTREACHED();
    return syncable::kInvalidMetaHandle;
  }

  entry.PutIsUnappliedUpdate(true);
  entry.PutServerVersion(GetNextRevision());

  return metahandle;
}

int64_t TestEntryFactory::CreateSyncedItem(const std::string& name,
                                           ModelType model_type,
                                           bool is_folder) {
  return CreateSyncedItem(name, model_type, is_folder,
                          sync_pb::EntitySpecifics());
}

int64_t TestEntryFactory::CreateSyncedItem(
    const std::string& name,
    ModelType model_type,
    bool is_folder,
    const sync_pb::EntitySpecifics& specifics) {
  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);

  // Use the type root if it exists or the real root otherwise.
  syncable::Entry root(&trans, syncable::GET_TYPE_ROOT, model_type);
  syncable::Id parent_id = root.good() ? root.GetId() : TestIdFactory::root();

  MutableEntry entry(&trans, syncable::CREATE, model_type, parent_id, name);
  if (!entry.good()) {
    NOTREACHED();
    return syncable::kInvalidMetaHandle;
  }

  PopulateEntry(parent_id, name, model_type, &entry);
  entry.PutIsDir(is_folder);
  entry.PutServerIsDir(is_folder);

  // Only rewrite the default specifics inside the entry (which have the model
  // type marker already) if |specifics| actually contains data.
  if (specifics.ByteSize() > 0) {
    entry.PutSpecifics(specifics);
    entry.PutServerSpecifics(specifics);
  } else {
    entry.PutServerSpecifics(entry.GetSpecifics());
  }

  return entry.GetMetahandle();
}

int64_t TestEntryFactory::CreateTombstone(const std::string& name,
                                          ModelType model_type) {
  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);

  // Use the type root if it exists or the real root otherwise.
  syncable::Entry root(&trans, syncable::GET_TYPE_ROOT, model_type);
  syncable::Id parent_id = root.good() ? root.GetId() : TestIdFactory::root();

  MutableEntry entry(&trans, syncable::CREATE, model_type, parent_id, name);
  if (!entry.good()) {
    NOTREACHED();
    return syncable::kInvalidMetaHandle;
  }

  PopulateEntry(parent_id, name, model_type, &entry);
  entry.PutIsDel(true);
  entry.PutServerIsDel(true);
  return entry.GetMetahandle();
}

int64_t TestEntryFactory::CreateTypeRootNode(ModelType model_type) {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, syncable::UNITTEST,
                                               directory_);
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  syncable::ModelNeutralMutableEntry entry(
      &trans, syncable::CREATE_NEW_TYPE_ROOT, model_type);
  DCHECK(entry.good());
  entry.PutServerIsDir(true);
  entry.PutUniqueServerTag(ModelTypeToRootTag(model_type));
  return entry.GetMetahandle();
}

int64_t TestEntryFactory::CreateUnappliedRootNode(ModelType model_type) {
  syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, directory_);
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  syncable::Id node_id = TestIdFactory::MakeServer("xyz");
  syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                               node_id);
  DCHECK(entry.good());
  // Make it look like sort of like a pending creation from the server.
  // The SERVER_PARENT_ID and UNIQUE_CLIENT_TAG aren't quite right, but
  // it's good enough for our purposes.
  entry.PutServerVersion(1);
  entry.PutIsUnappliedUpdate(true);
  entry.PutServerIsDir(false);
  entry.PutServerParentId(TestIdFactory::root());
  entry.PutServerSpecifics(specifics);
  entry.PutNonUniqueName("xyz");

  return entry.GetMetahandle();
}

bool TestEntryFactory::SetServerSpecificsForItem(
    int64_t meta_handle,
    const sync_pb::EntitySpecifics specifics) {
  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    return false;
  }
  entry.PutServerSpecifics(specifics);
  entry.PutIsUnappliedUpdate(true);
  return true;
}

bool TestEntryFactory::SetLocalSpecificsForItem(
    int64_t meta_handle,
    const sync_pb::EntitySpecifics specifics) {
  syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    return false;
  }
  entry.PutSpecifics(specifics);
  entry.PutIsUnsynced(true);
  return true;
}

const sync_pb::EntitySpecifics& TestEntryFactory::GetServerSpecificsForItem(
    int64_t meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.GetServerSpecifics();
}

const sync_pb::EntitySpecifics& TestEntryFactory::GetLocalSpecificsForItem(
    int64_t meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.GetSpecifics();
}


bool TestEntryFactory::GetIsUnsyncedForItem(int64_t meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    NOTREACHED();
    return false;
  }
  return entry.GetIsUnsynced();
}

bool TestEntryFactory::GetIsUnappliedForItem(int64_t meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    NOTREACHED();
    return false;
  }
  return entry.GetIsUnappliedUpdate();
}

int64_t TestEntryFactory::GetNextRevision() {
  return next_revision_++;
}

void TestEntryFactory::PopulateEntry(const syncable::Id& parent_id,
                                     const std::string& name,
                                     ModelType model_type,
                                     MutableEntry* entry) {
  syncable::Id item_id(TestIdFactory::MakeServer(name));
  int64_t version = GetNextRevision();
  base::Time now = base::Time::Now();

  entry->PutId(item_id);
  entry->PutCtime(now);
  entry->PutMtime(now);
  entry->PutUniqueClientTag(
      ClientTagHash::FromUnhashed(model_type, name).value());
  entry->PutBaseVersion(version);
  entry->PutIsUnsynced(false);
  entry->PutNonUniqueName(name);
  entry->PutIsDir(false);
  entry->PutIsDel(false);
  entry->PutParentId(parent_id);

  entry->PutServerCtime(now);
  entry->PutServerMtime(now);
  entry->PutServerVersion(version);
  entry->PutIsUnappliedUpdate(false);
  entry->PutServerNonUniqueName(name);
  entry->PutServerParentId(parent_id);
  entry->PutServerIsDir(false);
  entry->PutServerIsDel(false);
}

}  // namespace syncer
