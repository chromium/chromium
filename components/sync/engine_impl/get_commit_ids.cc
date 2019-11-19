// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/get_commit_ids.h"

#include <set>

#include "base/macros.h"
#include "base/stl_util.h"
#include "components/sync/engine_impl/syncer_util.h"
#include "components/sync/syncable/directory_cryptographer.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/nigori_handler.h"
#include "components/sync/syncable/nigori_util.h"
#include "components/sync/syncable/syncable_base_transaction.h"
#include "components/sync/syncable/syncable_util.h"

namespace syncer {

using syncable::Directory;
using syncable::Entry;

namespace {

bool IsEntryInConflict(const Entry& entry) {
  if (entry.GetIsUnsynced() && entry.GetServerVersion() > 0 &&
      (entry.GetServerVersion() > entry.GetBaseVersion())) {
    // The local and server versions don't match. The item must be in
    // conflict, so there's no point in attempting to commit.
    DCHECK(entry.GetIsUnappliedUpdate());
    DVLOG(1) << "Excluding entry from commit due to version mismatch " << entry;
    return true;
  }
  return false;
}

// An entry may not commit if any are true:
// 1. It requires encryption (either the type is encrypted but a passphrase
//    is missing from the cryptographer, or the entry itself wasn't properly
//    encrypted).
// 2. It's type is currently throttled.
// 3. It's a delete but has not been committed.
bool MayEntryCommit(ModelTypeSet requested_types,
                    ModelTypeSet encrypted_types,
                    bool passphrase_missing,
                    const Entry& entry) {
  DCHECK(entry.GetIsUnsynced());

  const ModelType type = entry.GetModelType();
  // We special case the nigori node because even though it is considered an
  // "encrypted type", not all nigori node changes require valid encryption
  // (ex: sync_tabs).
  if ((type != NIGORI) && encrypted_types.Has(type) &&
      (passphrase_missing ||
       syncable::EntryNeedsEncryption(encrypted_types, entry))) {
    // This entry requires encryption but is not properly encrypted (possibly
    // due to the cryptographer not being initialized or the user hasn't
    // provided the most recent passphrase).
    DVLOG(1) << "Excluding entry from commit due to lack of encryption "
             << entry;
    return false;
  }

  // Ignore it if it's not in our set of requested types.
  if (!requested_types.Has(type))
    return false;

  if (entry.GetIsDel() && !entry.GetId().ServerKnows()) {
    // New clients (following the resolution of crbug.com/125381) should not
    // create such items. Old clients may have left some in the database
    // (crbug.com/132905), but we should now be cleaning them on startup.
    NOTREACHED() << "Found deleted and unsynced local item: " << entry;
    return false;
  }

  // Extra validity checks.
  syncable::Id id = entry.GetId();
  if (id == entry.GetParentId()) {
    DCHECK(id.IsRoot()) << "Non-root item is self parenting." << entry;
    // If the root becomes unsynced it can cause us problems.
    NOTREACHED() << "Root item became unsynced " << entry;
    return false;
  }

  if (entry.IsRoot()) {
    NOTREACHED() << "Permanent item became unsynced " << entry;
    return false;
  }

  DVLOG(2) << "Entry is ready for commit: " << entry;
  return true;
}

bool SupportsHierarchy(const Entry& item) {
  // Types with explicit server supported hierarchy only.
  return IsTypeWithServerGeneratedRoot(item.GetModelType());
}

// Iterates over children of items from |conflicted_items| list that are in
// |ready_unsynced_set|, excludes them from |ready_unsynced_set| and adds them
// to |excluded_items| list.
void ExcludeChildren(syncable::BaseTransaction* trans,
                     const std::vector<int64_t>& conflicted_items,
                     std::vector<int64_t>* excluded_items,
                     std::set<int64_t>* ready_unsynced_set) {
  for (const int64_t& handle : conflicted_items) {
    Entry entry(trans, syncable::GET_BY_HANDLE, handle);

    if (!entry.GetIsDir() || entry.GetIsDel())
      continue;

    std::vector<int64_t> children;
    entry.GetChildHandles(&children);

    for (const int64_t& child_handle : children) {
      // Collect all child handles that are in |ready_unsynced_set|.
      auto ready_iter = ready_unsynced_set->find(child_handle);
      if (ready_iter != ready_unsynced_set->end()) {
        // Remove this entry from |ready_unsynced_set| and add it
        // to |excluded_items|.
        ready_unsynced_set->erase(ready_iter);
        excluded_items->push_back(child_handle);
      }
    }
  }
}

// This class helps to implement OrderCommitIds(). Its members track the
// progress of a traversal while its methods extend it. It can return early if
// the traversal reaches the desired size before the full traversal is complete.
class Traversal {
 public:
  Traversal(syncable::BaseTransaction* trans,
            int64_t max_entries,
            Directory::Metahandles* out);
  ~Traversal();

  // First step of traversal building. Adds non-deleted items in order.
  void AddCreatesAndMoves(const std::set<int64_t>& ready_unsynced_set);

  // Second step of traverals building. Appends deleted items.
  void AddDeletes(const std::set<int64_t>& ready_unsynced_set);

 private:
  // The following functions do not modify the traversal directly. They return
  // their results in the |result| vector instead.
  bool TryAddUncommittedParents(const std::set<int64_t>& ready_unsynced_set,
                                const Entry& item,
                                Directory::Metahandles* result) const;

  bool TryAddItem(const std::set<int64_t>& ready_unsynced_set,
                  const Entry& item,
                  Directory::Metahandles* result) const;

  void AddDeletedParents(const std::set<int64_t>& ready_unsynced_set,
                         const Entry& item,
                         const Directory::Metahandles& traversed,
                         Directory::Metahandles* result) const;

  // Returns true if we've collected enough items.
  bool IsFull() const;

  // Returns true if the specified handle is already in the traversal.
  bool HaveItem(int64_t handle) const;

  // Adds the specified handles to the traversal.
  void AppendManyToTraversal(const Directory::Metahandles& handles);

  // Adds the specified handle to the traversal.
  void AppendToTraversal(int64_t handle);

  Directory::Metahandles* out_;
  std::set<int64_t> added_handles_;
  const size_t max_entries_;
  syncable::BaseTransaction* trans_;

  DISALLOW_COPY_AND_ASSIGN(Traversal);
};

Traversal::Traversal(syncable::BaseTransaction* trans,
                     int64_t max_entries,
                     Directory::Metahandles* out)
    : out_(out), max_entries_(max_entries), trans_(trans) {}

Traversal::~Traversal() {}

bool Traversal::TryAddUncommittedParents(
    const std::set<int64_t>& ready_unsynced_set,
    const Entry& item,
    Directory::Metahandles* result) const {
  DCHECK(SupportsHierarchy(item));
  Directory::Metahandles dependencies;
  syncable::Id parent_id = item.GetParentId();

  // Climb the tree adding entries leaf -> root.
  while (!parent_id.ServerKnows()) {
    Entry parent(trans_, syncable::GET_BY_ID, parent_id);

    // This apparently does happen, see crbug.com/711381. Someone is violating
    // some constraint and some ancestor isn't current present in the directory
    // while the child is. Because we do not know where this comes from, be
    // defensive and skip this inclusion instead.
    if (!parent.good()) {
      DVLOG(1) << "Bad user-only parent in item path with id " << parent_id;
      return false;
    }

    int64_t handle = parent.GetMetahandle();
    if (HaveItem(handle)) {
      // We've already added this parent (and therefore all of its parents).
      // We can return early.
      break;
    }

    if (!TryAddItem(ready_unsynced_set, parent, &dependencies)) {
      // The parent isn't in |ready_unsynced_set|.
      break;
    }

    parent_id = parent.GetParentId();
  }

  // Reverse what we added to get the correct order.
  result->insert(result->end(), dependencies.rbegin(), dependencies.rend());
  return true;
}

// Adds the given item to the list if it is unsynced and ready for commit.
bool Traversal::TryAddItem(const std::set<int64_t>& ready_unsynced_set,
                           const Entry& item,
                           Directory::Metahandles* result) const {
  DCHECK(item.GetIsUnsynced());
  int64_t item_handle = item.GetMetahandle();
  if (ready_unsynced_set.count(item_handle) != 0) {
    result->push_back(item_handle);
    return true;
  }
  return false;
}

// Traverses the tree from bottom to top, adding the deleted parents of the
// given |item|. Stops traversing if it encounters a non-deleted node, or
// a node that was already listed in the |traversed| list.
//
// The result list is reversed before it is returned, so the resulting
// traversal is in top to bottom order. Also note that this function appends
// to the result list without clearing it.
void Traversal::AddDeletedParents(const std::set<int64_t>& ready_unsynced_set,
                                  const Entry& item,
                                  const Directory::Metahandles& traversed,
                                  Directory::Metahandles* result) const {
  DCHECK(SupportsHierarchy(item));
  Directory::Metahandles dependencies;
  syncable::Id parent_id = item.GetParentId();

  // Climb the tree adding entries leaf -> root.
  while (!parent_id.IsRoot()) {
    Entry parent(trans_, syncable::GET_BY_ID, parent_id);

    if (!parent.good()) {
      // This is valid because the parent could have gone away a long time ago.
      //
      // Consider the case where a folder is server-unknown and locally
      // deleted, and has a child that is server-known, deleted, and unsynced.
      // The parent could be dropped from memory at any time, but its child
      // needs to be committed first.
      break;
    }
    int64_t handle = parent.GetMetahandle();
    if (!parent.GetIsUnsynced()) {
      // In some rare cases, our parent can be both deleted and unsynced.
      // (ie. the server-unknown parent case).
      break;
    }
    if (!parent.GetIsDel()) {
      // We're not interested in non-deleted parents.
      break;
    }
    if (base::Contains(traversed, handle)) {
      // We've already added this parent (and therefore all of its parents).
      // We can return early.
      break;
    }

    if (!TryAddItem(ready_unsynced_set, parent, &dependencies)) {
      // The parent isn't in ready_unsynced_set.
      break;
    }

    parent_id = parent.GetParentId();
  }

  // Reverse what we added to get the correct order.
  result->insert(result->end(), dependencies.rbegin(), dependencies.rend());
}

bool Traversal::IsFull() const {
  return out_->size() >= max_entries_;
}

bool Traversal::HaveItem(int64_t handle) const {
  return added_handles_.find(handle) != added_handles_.end();
}

void Traversal::AppendManyToTraversal(const Directory::Metahandles& handles) {
  out_->insert(out_->end(), handles.begin(), handles.end());
  added_handles_.insert(handles.begin(), handles.end());
}

void Traversal::AppendToTraversal(int64_t handle) {
  out_->push_back(handle);
  added_handles_.insert(handle);
}

void Traversal::AddCreatesAndMoves(
    const std::set<int64_t>& ready_unsynced_set) {
  // Add moves and creates, and prepend their uncommitted parents.
  for (const int64_t& handle : ready_unsynced_set) {
    if (IsFull()) {
      break;
    }
    if (HaveItem(handle))
      continue;

    Entry entry(trans_, syncable::GET_BY_HANDLE, handle);
    if (!entry.GetIsDel()) {
      if (SupportsHierarchy(entry)) {
        // We only commit an item + its dependencies if it and all its
        // dependencies are not in conflict.
        Directory::Metahandles item_dependencies;

        // If we fail to add a required parent, give up on this entry.
        if (!TryAddUncommittedParents(ready_unsynced_set, entry,
                                      &item_dependencies)) {
          continue;
        }

        // Okay if this fails, the parents were still valid.
        TryAddItem(ready_unsynced_set, entry, &item_dependencies);
        AppendManyToTraversal(item_dependencies);
      } else {
        // No hierarchy dependencies, just commit the item itself.
        AppendToTraversal(handle);
      }
    }
  }

  // It's possible that we over committed while trying to expand dependent
  // items. If so, truncate the set down to the allowed size. This is safe
  // because we've ordered such that ancestors come before children.
  if (out_->size() > max_entries_)
    out_->resize(max_entries_);
}

void Traversal::AddDeletes(const std::set<int64_t>& ready_unsynced_set) {
  Directory::Metahandles deletion_list;

  // Note: we iterate over all the unsynced set, regardless of the max size.
  // The max size is only enforced after the top-to-bottom order has been
  // reversed, in order to ensure children are always deleted before parents.
  // We cannot bail early when full because we need to guarantee that children
  // are always deleted before parents/ancestors.
  for (const int64_t& handle : ready_unsynced_set) {
    if (HaveItem(handle))
      continue;

    if (base::Contains(deletion_list, handle)) {
      continue;
    }

    Entry entry(trans_, syncable::GET_BY_HANDLE, handle);

    if (entry.GetIsDel()) {
      if (SupportsHierarchy(entry)) {
        Directory::Metahandles parents;
        AddDeletedParents(ready_unsynced_set, entry, deletion_list, &parents);
        // Append parents and children in top to bottom order.
        deletion_list.insert(deletion_list.end(), parents.begin(),
                             parents.end());
      }
      deletion_list.push_back(handle);
    }
  }

  // We've been gathering deletions in top to bottom order. Now we reverse the
  // order as we prepare the list.
  std::reverse(deletion_list.begin(), deletion_list.end());
  AppendManyToTraversal(deletion_list);

  // It's possible that we over committed while trying to expand dependent
  // items. If so, truncate the set down to the allowed size. This is safe
  // because of the reverse above, which should guarantee the leafy nodes are
  // in the front of the ancestors nodes.
  if (out_->size() > max_entries_)
    out_->resize(max_entries_);
}

// Excludes ancestors of deleted conflicted items from |ready_unsynced_set|.
void ExcludeDeletedAncestors(
    syncable::BaseTransaction* trans,
    const std::vector<int64_t>& deleted_conflicted_items,
    std::set<int64_t>* ready_unsynced_set) {
  for (const int64_t& deleted_conflicted_handle : deleted_conflicted_items) {
    Entry item(trans, syncable::GET_BY_HANDLE, deleted_conflicted_handle);
    syncable::Id parent_id = item.GetParentId();
    DCHECK(!parent_id.IsNull());

    while (!parent_id.IsRoot()) {
      Entry parent(trans, syncable::GET_BY_ID, parent_id);
      DCHECK(parent.good()) << "Bad user-only parent in item path.";
      int64_t handle = parent.GetMetahandle();

      if (!parent.GetIsDel())
        break;

      auto ready_iter = ready_unsynced_set->find(handle);
      if (ready_iter == ready_unsynced_set->end())
        break;

      // Remove this entry from |ready_unsynced_set|.
      ready_unsynced_set->erase(ready_iter);
      parent_id = parent.GetParentId();
    }
  }
}

// Filters |unsynced_handles| to remove all entries that do not belong to the
// specified |requested_types|, or are not eligible for a commit at this time.
void FilterUnreadyEntries(syncable::BaseTransaction* trans,
                          ModelTypeSet requested_types,
                          ModelTypeSet encrypted_types,
                          bool passphrase_missing,
                          const Directory::Metahandles& unsynced_handles,
                          std::set<int64_t>* ready_unsynced_set) {
  std::vector<int64_t> deleted_conflicted_items;
  std::vector<int64_t> conflicted_items;

  // Go over all unsynced handles, filter the ones that might be committed based
  // on type / encryption, then based on whether they are in conflict add them
  // to either |ready_unsynced_set| or one of the conflicted lists.
  for (const int64_t& handle : unsynced_handles) {
    Entry entry(trans, syncable::GET_BY_HANDLE, handle);
    // TODO(maniscalco): While we check if entry is ready to be committed, we
    // also need to check that all of its ancestors (parents, transitive) are
    // ready to be committed.
    if (MayEntryCommit(requested_types, encrypted_types, passphrase_missing,
                       entry)) {
      if (IsEntryInConflict(entry)) {
        // Conflicting hierarchical entries might prevent their ancestors or
        // descendants from being committed.
        if (SupportsHierarchy(entry)) {
          if (entry.GetIsDel()) {
            deleted_conflicted_items.push_back(handle);
          } else if (entry.GetIsDir()) {
            // Populate the initial version of |conflicted_items| with folder
            // items that are in conflict.
            conflicted_items.push_back(handle);
          }
        }
      } else {
        ready_unsynced_set->insert(handle);
      }
    }
  }

  // If there are any deleted conflicted entries, remove their deleted ancestors
  // from |ready_unsynced_set| as well.
  ExcludeDeletedAncestors(trans, deleted_conflicted_items, ready_unsynced_set);

  // Starting with conflicted_items containing conflicted folders go down and
  // exclude all descendants from |ready_unsynced_set|.
  while (!conflicted_items.empty()) {
    std::vector<int64_t> new_list;
    ExcludeChildren(trans, conflicted_items, &new_list, ready_unsynced_set);
    conflicted_items.swap(new_list);
  }
}

// Given a set of commit metahandles that are ready for commit
// (|ready_unsynced_set|), sorts these into commit order and places up to
// |max_entries| of them in the output parameter |out|.
//
// See the header file for an explanation of commit ordering.
void OrderCommitIds(syncable::BaseTransaction* trans,
                    size_t max_entries,
                    const std::set<int64_t>& ready_unsynced_set,
                    Directory::Metahandles* out) {
  // Commits follow these rules:
  // 1. Moves or creates are preceded by needed folder creates, from
  //    root to leaf.
  // 2. Moves/Creates before deletes.
  // 3. Deletes, collapsed.
  // We commit deleted moves under deleted items as moves when collapsing
  // delete trees.

  Traversal traversal(trans, max_entries, out);

  // Add moves and creates, and prepend their uncommitted parents.
  traversal.AddCreatesAndMoves(ready_unsynced_set);

  // Add all deletes.
  traversal.AddDeletes(ready_unsynced_set);
}

}  // namespace

void GetCommitIdsForType(syncable::BaseTransaction* trans,
                         ModelType type,
                         size_t max_entries,
                         Directory::Metahandles* out) {
  Directory* dir = trans->directory();

  // Gather the full set of unsynced items and store it in the cycle. They
  // are not in the correct order for commit.
  std::set<int64_t> ready_unsynced_set;
  Directory::Metahandles all_unsynced_handles;
  GetUnsyncedEntries(trans, &all_unsynced_handles);

  ModelTypeSet encrypted_types;
  bool passphrase_missing = false;
  const DirectoryCryptographer* cryptographer =
      dir->GetNigoriHandler()->GetDirectoryCryptographer(trans);
  if (cryptographer) {
    encrypted_types = dir->GetNigoriHandler()->GetEncryptedTypes(trans);
    passphrase_missing = cryptographer->has_pending_keys();
  }

  // We filter out all unready entries from the set of unsynced handles. This
  // new set of ready and unsynced items is then what we use to determine what
  // is a candidate for commit. The caller is responsible for ensuring that no
  // throttled types are included among the requested_types.
  FilterUnreadyEntries(trans, ModelTypeSet(type), encrypted_types,
                       passphrase_missing, all_unsynced_handles,
                       &ready_unsynced_set);

  OrderCommitIds(trans, max_entries, ready_unsynced_set, out);

  for (const int64_t& handle : *out) {
    DVLOG(1) << "Debug commit batch result:" << handle;
  }
}

}  // namespace syncer
