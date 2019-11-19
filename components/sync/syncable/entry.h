// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_ENTRY_H_
#define COMPONENTS_SYNC_SYNCABLE_ENTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "components/sync/syncable/entry_kernel.h"

namespace syncer {

class Cryptographer;
class ReadNode;

namespace syncable {

class Directory;
class BaseTransaction;

// A read-only meta entry
// Instead of:
//   Entry e = transaction.GetById(id);
// use:
//   Entry e(transaction, GET_BY_ID, id);
//
// Why?  The former would require a copy constructor, and it would be difficult
// to enforce that an entry never outlived its transaction if there were a copy
// constructor.
enum GetById { GET_BY_ID };

enum GetByClientTag { GET_BY_CLIENT_TAG };

enum GetByServerTag {
  // Server tagged items are deprecated for all types but bookmarks.
  GET_BY_SERVER_TAG
};

enum GetTypeRoot { GET_TYPE_ROOT };

enum GetByHandle { GET_BY_HANDLE };

class Entry {
 public:
  // After constructing, you must check good() to test whether the Get
  // succeeded.
  Entry(BaseTransaction* trans, GetByHandle, int64_t handle);
  Entry(BaseTransaction* trans, GetById, const Id& id);
  Entry(BaseTransaction* trans, GetTypeRoot, ModelType type);
  Entry(BaseTransaction* trans, GetByClientTag, const std::string& tag);

  // This lookup function is deprecated.  All types except bookmarks can use
  // the GetTypeRoot variant instead.
  Entry(BaseTransaction* trans, GetByServerTag, const std::string& tag);

  bool good() const { return 0 != kernel_; }

  BaseTransaction* trans() const { return basetrans_; }

  // Field accessors.
  int64_t GetMetahandle() const {
    DCHECK(kernel_);
    return kernel_->ref(META_HANDLE);
  }

  int64_t GetBaseVersion() const {
    DCHECK(kernel_);
    return kernel_->ref(BASE_VERSION);
  }

  int64_t GetServerVersion() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_VERSION);
  }

  int64_t GetLocalExternalId() const {
    DCHECK(kernel_);
    return kernel_->ref(LOCAL_EXTERNAL_ID);
  }

  int64_t GetTransactionVersion() const {
    DCHECK(kernel_);
    return kernel_->ref(TRANSACTION_VERSION);
  }

  const base::Time& GetMtime() const {
    DCHECK(kernel_);
    return kernel_->ref(MTIME);
  }

  const base::Time& GetServerMtime() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_MTIME);
  }

  const base::Time& GetCtime() const {
    DCHECK(kernel_);
    return kernel_->ref(CTIME);
  }

  const base::Time& GetServerCtime() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_CTIME);
  }

  const Id& GetId() const {
    DCHECK(kernel_);
    return kernel_->ref(ID);
  }

  const Id& GetParentId() const {
    DCHECK(kernel_);
    return kernel_->ref(PARENT_ID);
  }

  Id GetServerParentId() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_PARENT_ID);
  }

  bool GetIsUnsynced() const {
    DCHECK(kernel_);
    return kernel_->ref(IS_UNSYNCED);
  }

  bool GetIsUnappliedUpdate() const {
    DCHECK(kernel_);
    return kernel_->ref(IS_UNAPPLIED_UPDATE);
  }

  bool GetIsDel() const {
    DCHECK(kernel_);
    return kernel_->ref(IS_DEL);
  }

  bool GetIsDir() const {
    DCHECK(kernel_);
    return kernel_->ref(IS_DIR);
  }

  bool GetServerIsDir() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_IS_DIR);
  }

  bool GetServerIsDel() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_IS_DEL);
  }

  const std::string& GetNonUniqueName() const {
    DCHECK(kernel_);
    return kernel_->ref(NON_UNIQUE_NAME);
  }

  const std::string& GetServerNonUniqueName() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_NON_UNIQUE_NAME);
  }

  const std::string& GetUniqueServerTag() const {
    DCHECK(kernel_);
    return kernel_->ref(UNIQUE_SERVER_TAG);
  }

  const std::string& GetUniqueClientTag() const {
    DCHECK(kernel_);
    return kernel_->ref(UNIQUE_CLIENT_TAG);
  }

  const std::string& GetUniqueBookmarkTag() const {
    DCHECK(kernel_);
    return kernel_->ref(UNIQUE_BOOKMARK_TAG);
  }

  const sync_pb::EntitySpecifics& GetSpecifics() const {
    DCHECK(kernel_);
    return kernel_->ref(SPECIFICS);
  }

  const sync_pb::EntitySpecifics& GetServerSpecifics() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_SPECIFICS);
  }

  const sync_pb::EntitySpecifics& GetBaseServerSpecifics() const {
    DCHECK(kernel_);
    return kernel_->ref(BASE_SERVER_SPECIFICS);
  }

  const UniquePosition& GetServerUniquePosition() const {
    DCHECK(kernel_);
    return kernel_->ref(SERVER_UNIQUE_POSITION);
  }

  const UniquePosition& GetUniquePosition() const {
    DCHECK(kernel_);
    return kernel_->ref(UNIQUE_POSITION);
  }

  bool GetSyncing() const;
  bool GetDirtySync() const;

  ModelType GetServerModelType() const;
  ModelType GetModelType() const;

  Id GetPredecessorId() const;
  Id GetSuccessorId() const;
  Id GetFirstChildId() const;
  int GetTotalNodeCount() const;

  int GetPositionIndex() const;

  // Returns a vector of this node's children's handles.
  // Clears |result| if there are no children.  If this node is of a type that
  // supports user-defined ordering then the resulting vector will be in the
  // proper order.
  void GetChildHandles(std::vector<int64_t>* result) const;

  inline bool ExistsOnClientBecauseNameIsNonEmpty() const {
    DCHECK(kernel_);
    return !kernel_->ref(NON_UNIQUE_NAME).empty();
  }

  inline bool IsRoot() const {
    DCHECK(kernel_);
    return kernel_->ref(ID).IsRoot();
  }

  // Returns true if this is an entry that is expected to maintain a certain
  // sort ordering relative to its siblings under the same parent.
  bool ShouldMaintainPosition() const;

  // Returns true if this is an entry that is expected to maintain hierarchy.
  // ie. Whether or not the PARENT_ID field contains useful information.
  bool ShouldMaintainHierarchy() const;

  Directory* dir() const;

  const EntryKernel GetKernelCopy() const { return *kernel_; }

  // Dumps all entry info into a DictionaryValue and returns it.
  std::unique_ptr<base::DictionaryValue> ToValue(
      const Cryptographer* cryptographer) const;

 protected:  // Don't allow creation on heap, except by sync API wrappers.
  void* operator new(size_t size) { return (::operator new)(size); }

  inline explicit Entry(BaseTransaction* trans)
      : basetrans_(trans), kernel_(nullptr) {}

 protected:
  BaseTransaction* const basetrans_;

  EntryKernel* kernel_;

 private:
  friend class syncer::syncable::Directory;
  friend class syncer::ReadNode;
  friend std::ostream& operator<<(std::ostream& s, const Entry& e);

  DISALLOW_COPY_AND_ASSIGN(Entry);
};

std::ostream& operator<<(std::ostream& os, const Entry& entry);

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_ENTRY_H_
