// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/syncable_util.h"

#include "base/location.h"
#include "base/logging.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_id.h"
#include "components/sync/syncable/syncable_write_transaction.h"

namespace syncer {
namespace syncable {

// Returns the number of unsynced entries.
int GetUnsyncedEntries(BaseTransaction* trans,
                       Directory::Metahandles* handles) {
  trans->directory()->GetUnsyncedMetaHandles(trans, handles);
  DVLOG_IF(1, !handles->empty()) << "Have " << handles->size()
                                 << " unsynced items.";
  return handles->size();
}

bool IsLegalNewParent(BaseTransaction* trans,
                      const Id& entry_id,
                      const Id& new_parent_id) {
  DCHECK(!entry_id.IsNull());
  DCHECK(!new_parent_id.IsNull());
  if (entry_id.IsRoot())
    return false;
  // we have to ensure that the entry is not an ancestor of the new parent.
  Id ancestor_id = new_parent_id;
  while (!ancestor_id.IsRoot()) {
    if (entry_id == ancestor_id)
      return false;
    Entry new_parent(trans, GET_BY_ID, ancestor_id);
    if (!SyncAssert(new_parent.good(), FROM_HERE, "Invalid new parent", trans))
      return false;
    ancestor_id = new_parent.GetParentId();
  }
  return true;
}

void ChangeEntryIDAndUpdateChildren(BaseWriteTransaction* trans,
                                    ModelNeutralMutableEntry* entry,
                                    const Id& new_id) {
  Id old_id = entry->GetId();
  if (!entry->PutId(new_id)) {
    Entry old_entry(trans, GET_BY_ID, new_id);
    DCHECK(old_entry.good());
    LOG(FATAL) << "Attempt to change ID to " << new_id
               << " conflicts with existing entry.\n\n"
               << *entry << "\n\n"
               << old_entry;
  }
  if (entry->GetIsDir()) {
    // Get all child entries of the old id.
    Directory::Metahandles children;
    trans->directory()->GetChildHandlesById(trans, old_id, &children);
    auto i = children.begin();
    while (i != children.end()) {
      ModelNeutralMutableEntry child_entry(trans, GET_BY_HANDLE, *i++);
      DCHECK(child_entry.good());
      // Change the parent ID of the entry unless it was unset (implicit)
      if (!child_entry.GetParentId().IsNull()) {
        // Use the unchecked setter here to avoid touching the child's
        // UNIQUE_POSITION field.  In this case, UNIQUE_POSITION among the
        // children will be valid after the loop, since we update all the
        // children at once.
        child_entry.PutParentIdPropertyOnly(new_id);
      }
    }
  }
}

// Function to handle runtime failures on syncable code. Rather than crashing,
// if the |condition| is false the following will happen:
// 1. Sets unrecoverable error on transaction.
// 2. Returns false.
bool SyncAssert(bool condition,
                const base::Location& location,
                const char* msg,
                BaseTransaction* trans) {
  if (!condition) {
    trans->OnUnrecoverableError(location, msg);
    return false;
  }
  return true;
}

}  // namespace syncable
}  // namespace syncer
