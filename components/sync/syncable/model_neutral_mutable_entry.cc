// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/model_neutral_mutable_entry.h"

#include <memory>
#include <utility>

#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/scoped_kernel_lock.h"
#include "components/sync/syncable/syncable_changes_version.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/syncable/syncable_write_transaction.h"

using std::string;

namespace syncer {

namespace syncable {

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans,
                                                   CreateNewUpdateItem,
                                                   const Id& id)
    : Entry(trans), base_write_transaction_(trans) {
  Entry same_id(trans, GET_BY_ID, id);
  kernel_ = nullptr;
  if (same_id.good()) {
    return;  // already have an item with this ID.
  }
  std::unique_ptr<EntryKernel> kernel(new EntryKernel());

  kernel->put(ID, id);
  kernel->put(META_HANDLE, trans->directory()->NextMetahandle());
  kernel->mark_dirty(&trans->directory()->kernel()->dirty_metahandles);
  kernel->put(IS_DEL, true);
  // We match the database defaults here
  kernel->put(BASE_VERSION, CHANGES_VERSION);
  kernel_ = kernel.get();
  if (!trans->directory()->InsertEntry(trans, std::move(kernel))) {
    kernel_ = nullptr;
    return;  // Failed inserting.
  }
  trans->TrackChangesTo(kernel_);
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans,
                                                   CreateNewTypeRoot,
                                                   ModelType type)
    : Entry(trans), base_write_transaction_(trans) {
  // We allow NIGORI because we allow SyncEncryptionHandler to restore a nigori
  // across Directory instances (see
  // SyncEncryptionHandler::RestoreNigoriForTesting()).
  if (type != NIGORI)
    DCHECK(IsTypeWithClientGeneratedRoot(type));
  Entry same_type_root(trans, GET_TYPE_ROOT, type);
  kernel_ = nullptr;
  if (same_type_root.good()) {
    return;  // already have a type root for the given type
  }

  std::unique_ptr<EntryKernel> kernel(new EntryKernel());

  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(type, &specifics);
  kernel->put(SPECIFICS, specifics);

  kernel->put(ID,
              syncable::Id::CreateFromClientString(ModelTypeToString(type)));
  kernel->put(META_HANDLE, trans->directory()->NextMetahandle());
  kernel->put(PARENT_ID, syncable::Id::GetRoot());
  kernel->put(BASE_VERSION, CHANGES_VERSION);
  kernel->put(NON_UNIQUE_NAME, ModelTypeToString(type));
  kernel->put(IS_DIR, true);

  kernel->mark_dirty(&trans->directory()->kernel()->dirty_metahandles);
  kernel_ = kernel.get();

  if (!trans->directory()->InsertEntry(trans, std::move(kernel))) {
    kernel_ = nullptr;
    return;  // Failed inserting.
  }
  trans->TrackChangesTo(kernel_);
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans,
                                                   GetById,
                                                   const Id& id)
    : Entry(trans, GET_BY_ID, id), base_write_transaction_(trans) {}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans,
                                                   GetByHandle,
                                                   int64_t metahandle)
    : Entry(trans, GET_BY_HANDLE, metahandle), base_write_transaction_(trans) {}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans,
                                                   GetByClientTag,
                                                   const std::string& tag)
    : Entry(trans, GET_BY_CLIENT_TAG, tag), base_write_transaction_(trans) {}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans,
                                                   GetTypeRoot,
                                                   ModelType type)
    : Entry(trans, GET_TYPE_ROOT, type), base_write_transaction_(trans) {}

void ModelNeutralMutableEntry::PutBaseVersion(int64_t value) {
  DCHECK(kernel_);
  if (kernel_->ref(BASE_VERSION) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    kernel_->put(BASE_VERSION, value);
    MarkDirty();
  }
}

void ModelNeutralMutableEntry::PutServerVersion(int64_t value) {
  DCHECK(kernel_);
  if (kernel_->ref(SERVER_VERSION) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    ScopedKernelLock lock(dir());
    kernel_->put(SERVER_VERSION, value);
    MarkDirty();
  }
}

void ModelNeutralMutableEntry::PutServerMtime(base::Time value) {
  DCHECK(kernel_);
  if (kernel_->ref(SERVER_MTIME) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    kernel_->put(SERVER_MTIME, value);
    MarkDirty();
  }
}

void ModelNeutralMutableEntry::PutServerCtime(base::Time value) {
  DCHECK(kernel_);
  if (kernel_->ref(SERVER_CTIME) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    kernel_->put(SERVER_CTIME, value);
    MarkDirty();
  }
}

bool ModelNeutralMutableEntry::PutId(const Id& value) {
  DCHECK(kernel_);
  if (kernel_->ref(ID) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    if (!dir()->ReindexId(base_write_transaction(), kernel_, value))
      return false;
    MarkDirty();
  }
  return true;
}

void ModelNeutralMutableEntry::PutServerParentId(const Id& value) {
  DCHECK(kernel_);
  if (kernel_->ref(SERVER_PARENT_ID) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    kernel_->put(SERVER_PARENT_ID, value);
    MarkDirty();
  }
}

bool ModelNeutralMutableEntry::PutIsUnsynced(bool value) {
  DCHECK(kernel_);
  if (kernel_->ref(IS_UNSYNCED) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    MetahandleSet* index = &dir()->kernel()->unsynced_metahandles;

    ScopedKernelLock lock(dir());
    if (value) {
      if (!SyncAssert(index->insert(kernel_->ref(META_HANDLE)).second,
                      FROM_HERE, "Could not insert",
                      base_write_transaction())) {
        return false;
      }
    } else {
      if (!SyncAssert(1U == index->erase(kernel_->ref(META_HANDLE)), FROM_HERE,
                      "Entry Not successfully erased",
                      base_write_transaction())) {
        return false;
      }
    }
    kernel_->put(IS_UNSYNCED, value);
    MarkDirty();
  }
  return true;
}

bool ModelNeutralMutableEntry::PutIsUnappliedUpdate(bool value) {
  DCHECK(kernel_);
  if (kernel_->ref(IS_UNAPPLIED_UPDATE) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    // Use kernel_->GetServerModelType() instead of
    // GetServerModelType() as we may trigger some DCHECKs in the
    // latter.
    MetahandleSet* index =
        &dir()
             ->kernel()
             ->unapplied_update_metahandles[kernel_->GetServerModelType()];

    ScopedKernelLock lock(dir());
    if (value) {
      if (!SyncAssert(index->insert(kernel_->ref(META_HANDLE)).second,
                      FROM_HERE, "Could not insert",
                      base_write_transaction())) {
        return false;
      }
    } else {
      if (!SyncAssert(1U == index->erase(kernel_->ref(META_HANDLE)), FROM_HERE,
                      "Entry Not successfully erased",
                      base_write_transaction())) {
        return false;
      }
    }
    kernel_->put(IS_UNAPPLIED_UPDATE, value);
    MarkDirty();
  }
  return true;
}

void ModelNeutralMutableEntry::PutServerIsDir(bool value) {
  DCHECK(kernel_);
  if (kernel_->ref(SERVER_IS_DIR) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    kernel_->put(SERVER_IS_DIR, value);
    MarkDirty();
  }
}

void ModelNeutralMutableEntry::PutServerIsDel(bool value) {
  DCHECK(kernel_);
  bool old_value = kernel_->ref(SERVER_IS_DEL);
  if (old_value != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    kernel_->put(SERVER_IS_DEL, value);
    MarkDirty();
  }
}

void ModelNeutralMutableEntry::PutServerNonUniqueName(
    const std::string& value) {
  DCHECK(kernel_);
  if (kernel_->ref(SERVER_NON_UNIQUE_NAME) != value) {
    base_write_transaction_->TrackChangesTo(kernel_);
    kernel_->put(SERVER_NON_UNIQUE_NAME, value);
    MarkDirty();
  }
}

bool ModelNeutralMutableEntry::PutUniqueServerTag(const string& new_tag) {
  if (new_tag == kernel_->ref(UNIQUE_SERVER_TAG)) {
    return true;
  }

  base_write_transaction_->TrackChangesTo(kernel_);
  ScopedKernelLock lock(dir());
  // Make sure your new value is not in there already.
  if (dir()->kernel()->server_tags_map.find(new_tag) !=
      dir()->kernel()->server_tags_map.end()) {
    DVLOG(1) << "Detected duplicate server tag";
    return false;
  }
  dir()->kernel()->server_tags_map.erase(kernel_->ref(UNIQUE_SERVER_TAG));
  kernel_->put(UNIQUE_SERVER_TAG, new_tag);
  MarkDirty();
  if (!new_tag.empty()) {
    dir()->kernel()->server_tags_map[new_tag] = kernel_;
  }

  return true;
}

bool ModelNeutralMutableEntry::PutUniqueClientTag(const string& new_tag) {
  if (new_tag == kernel_->ref(UNIQUE_CLIENT_TAG)) {
    return true;
  }

  base_write_transaction_->TrackChangesTo(kernel_);
  ScopedKernelLock lock(dir());
  // Make sure your new value is not in there already.
  if (dir()->kernel()->client_tags_map.find(new_tag) !=
      dir()->kernel()->client_tags_map.end()) {
    DVLOG(1) << "Detected duplicate client tag";
    return false;
  }
  dir()->kernel()->client_tags_map.erase(kernel_->ref(UNIQUE_CLIENT_TAG));
  kernel_->put(UNIQUE_CLIENT_TAG, new_tag);
  MarkDirty();
  if (!new_tag.empty()) {
    dir()->kernel()->client_tags_map[new_tag] = kernel_;
  }

  return true;
}

void ModelNeutralMutableEntry::PutUniqueBookmarkTag(const std::string& tag) {
  // This unique tag will eventually be used as the unique suffix when adjusting
  // this bookmark's position.  Let's make sure it's a valid suffix.
  if (!UniquePosition::IsValidSuffix(tag)) {
    NOTREACHED();
    return;
  }

  // TODO(stanisc): Does this need a call to TrackChangesTo?

  if (!kernel_->ref(UNIQUE_BOOKMARK_TAG).empty() &&
      tag != kernel_->ref(UNIQUE_BOOKMARK_TAG)) {
    // There are two scenarios where our tag is expected to change.  The first
    // scenario occurs when our current tag is a non-correct tag assigned during
    // the UniquePosition migration. The second is when the local sync backend
    // database has been deleted ans is being recreated from the current client.
    std::string migration_generated_tag = GenerateSyncableBookmarkHash(
        std::string(), kernel_->ref(ID).GetServerId());
    DLOG_IF(WARNING,
            migration_generated_tag == kernel_->ref(UNIQUE_BOOKMARK_TAG))
        << "Unique bookmark tag mismatch!";
  }

  kernel_->put(UNIQUE_BOOKMARK_TAG, tag);
  MarkDirty();
}

void ModelNeutralMutableEntry::PutServerSpecifics(
    const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);

  // Purposefully crash if we have client only data, as this could result in
  // sending password in plain text.
  CHECK(!value.password().has_client_only_encrypted_data());

  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  const std::string& serialized_value = value.SerializeAsString();
  if (serialized_value != kernel_->ref(SERVER_SPECIFICS).SerializeAsString()) {
    base_write_transaction_->TrackChangesTo(kernel_);
    if (kernel_->ref(IS_UNAPPLIED_UPDATE)) {
      // Remove ourselves from unapplied_update_metahandles with our
      // old server type.
      const ModelType old_server_type = kernel_->GetServerModelType();
      const int64_t metahandle = kernel_->ref(META_HANDLE);
      size_t erase_count =
          dir()->kernel()->unapplied_update_metahandles[old_server_type].erase(
              metahandle);
      DCHECK_EQ(erase_count, 1u);
    }

    // Check for potential sharing - SERVER_SPECIFICS is often
    // copied from SPECIFICS.
    if (serialized_value == kernel_->ref(SPECIFICS).SerializeAsString()) {
      kernel_->copy(SPECIFICS, SERVER_SPECIFICS);
    } else {
      kernel_->put(SERVER_SPECIFICS, value);
    }
    MarkDirty();

    if (kernel_->ref(IS_UNAPPLIED_UPDATE)) {
      // Add ourselves back into unapplied_update_metahandles with our
      // new server type.
      const ModelType new_server_type = kernel_->GetServerModelType();
      const int64_t metahandle = kernel_->ref(META_HANDLE);
      dir()->kernel()->unapplied_update_metahandles[new_server_type].insert(
          metahandle);
    }
  }
}

void ModelNeutralMutableEntry::PutBaseServerSpecifics(
    const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);

  // Purposefully crash if we have client only data, as this could result in
  // sending password in plain text.
  CHECK(!value.password().has_client_only_encrypted_data());

  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  const std::string& serialized_value = value.SerializeAsString();
  if (serialized_value !=
      kernel_->ref(BASE_SERVER_SPECIFICS).SerializeAsString()) {
    base_write_transaction_->TrackChangesTo(kernel_);
    // Check for potential sharing - BASE_SERVER_SPECIFICS is often
    // copied from SERVER_SPECIFICS.
    if (serialized_value ==
        kernel_->ref(SERVER_SPECIFICS).SerializeAsString()) {
      kernel_->copy(SERVER_SPECIFICS, BASE_SERVER_SPECIFICS);
    } else {
      kernel_->put(BASE_SERVER_SPECIFICS, value);
    }
    MarkDirty();
  }
}

void ModelNeutralMutableEntry::PutServerUniquePosition(
    const UniquePosition& value) {
  DCHECK(kernel_);
  if (!kernel_->ref(SERVER_UNIQUE_POSITION).Equals(value)) {
    base_write_transaction_->TrackChangesTo(kernel_);
    // We should never overwrite a valid position with an invalid one.
    DCHECK(value.IsValid());
    ScopedKernelLock lock(dir());
    kernel_->put(SERVER_UNIQUE_POSITION, value);
    MarkDirty();
  }
}

void ModelNeutralMutableEntry::PutSyncing(bool value) {
  kernel_->put(SYNCING, value);
}

void ModelNeutralMutableEntry::PutDirtySync(bool value) {
  DCHECK(!value || GetSyncing());
  kernel_->put(DIRTY_SYNC, value);
}

void ModelNeutralMutableEntry::PutParentIdPropertyOnly(const Id& parent_id) {
  base_write_transaction_->TrackChangesTo(kernel_);
  dir()->ReindexParentId(base_write_transaction(), kernel_, parent_id);
  MarkDirty();
}

void ModelNeutralMutableEntry::UpdateTransactionVersion(int64_t value) {
  kernel_->put(TRANSACTION_VERSION, value);
  MarkDirty();
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans)
    : Entry(trans), base_write_transaction_(trans) {}

void ModelNeutralMutableEntry::MarkDirty() {
  kernel_->mark_dirty(&dir()->kernel()->dirty_metahandles);
}

}  // namespace syncable

}  // namespace syncer
