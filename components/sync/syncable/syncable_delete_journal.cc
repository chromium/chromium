// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/syncable_delete_journal.h"

#include <stdint.h>

#include <utility>

#include "components/sync/base/model_type.h"

namespace syncer {
namespace syncable {

DeleteJournal::DeleteJournal(std::unique_ptr<JournalIndex> initial_journal) {
  DCHECK(initial_journal);
  delete_journals_.swap(*initial_journal);
}

DeleteJournal::~DeleteJournal() {}

size_t DeleteJournal::GetDeleteJournalSize(BaseTransaction* trans) const {
  DCHECK(trans);
  return delete_journals_.size();
}

void DeleteJournal::UpdateDeleteJournalForServerDelete(
    BaseTransaction* trans,
    bool was_deleted,
    const EntryKernel& entry) {
  DCHECK(trans);

  // Should be sufficient to check server type only but check for local
  // type too because of incomplete test setup.
  if (!(IsDeleteJournalEnabled(entry.GetServerModelType()) ||
        IsDeleteJournalEnabled(
            GetModelTypeFromSpecifics(entry.ref(SPECIFICS))))) {
    return;
  }

  auto it = delete_journals_.find(&entry);

  if (entry.ref(SERVER_IS_DEL)) {
    if (it == delete_journals_.end()) {
      // New delete.
      auto entry_copy = std::make_unique<EntryKernel>(entry);
      delete_journals_to_purge_.erase(entry_copy->ref(META_HANDLE));
      AddEntryToJournalIndex(&delete_journals_, std::move(entry_copy));
    }
  } else {
    // Undelete. This could happen in two cases:
    // * An entry was deleted then undeleted, i.e. server delete was
    //   overwritten because of entry has unsynced data locally.
    // * A data type was broken, i.e. encountered unrecoverable error, in last
    //   sync session and all its entries were duplicated in delete journals.
    //   On restart, entries are recreated from downloads and recreation calls
    //   UpdateDeleteJournals() to remove live entries from delete journals,
    //   thus only deleted entries remain in journals.
    if (it != delete_journals_.end()) {
      delete_journals_to_purge_.insert((*it).first->ref(META_HANDLE));
      delete_journals_.erase(it);
    } else if (was_deleted) {
      delete_journals_to_purge_.insert(entry.ref(META_HANDLE));
    }
  }
}

void DeleteJournal::GetDeleteJournals(BaseTransaction* trans,
                                      ModelType type,
                                      EntryKernelSet* deleted_entries) {
  DCHECK(trans);
  for (auto it = delete_journals_.begin(); it != delete_journals_.end(); ++it) {
    if ((*it).first->GetServerModelType() == type ||
        GetModelTypeFromSpecifics((*it).first->ref(SPECIFICS)) == type) {
      deleted_entries->insert((*it).first);
    }
  }
  passive_delete_journal_types_.Put(type);
}

void DeleteJournal::PurgeDeleteJournals(BaseTransaction* trans,
                                        const MetahandleSet& to_purge) {
  DCHECK(trans);
  auto it = delete_journals_.begin();
  while (it != delete_journals_.end()) {
    int64_t handle = (*it).first->ref(META_HANDLE);
    if (to_purge.count(handle)) {
      delete_journals_.erase(it++);
    } else {
      ++it;
    }
  }
  delete_journals_to_purge_.insert(to_purge.begin(), to_purge.end());
}

void DeleteJournal::TakeSnapshotAndClear(BaseTransaction* trans,
                                         OwnedEntryKernelSet* journal_entries,
                                         MetahandleSet* journals_to_purge) {
  DCHECK(trans);
  // Move passive delete journals to snapshot. Will copy back if snapshot fails
  // to save.
  auto it = delete_journals_.begin();
  while (it != delete_journals_.end()) {
    if (passive_delete_journal_types_.Has((*it).first->GetServerModelType()) ||
        passive_delete_journal_types_.Has(
            GetModelTypeFromSpecifics((*it).first->ref(SPECIFICS)))) {
      journal_entries->insert(std::move((*it).second));
      delete_journals_.erase(it++);
    } else {
      ++it;
    }
  }
  *journals_to_purge = delete_journals_to_purge_;
  delete_journals_to_purge_.clear();
}

void DeleteJournal::AddJournalBatch(BaseTransaction* trans,
                                    const OwnedEntryKernelSet& entries) {
  DCHECK(trans);
  EntryKernel needle;
  for (auto& entry : entries) {
    needle.put(ID, entry->ref(ID));
    if (delete_journals_.find(&needle) == delete_journals_.end()) {
      auto entry_copy = std::make_unique<EntryKernel>(*entry);
      AddEntryToJournalIndex(&delete_journals_, std::move(entry_copy));
    }
    delete_journals_to_purge_.erase(entry->ref(META_HANDLE));
  }
}

/* static */
bool DeleteJournal::IsDeleteJournalEnabled(ModelType type) {
  switch (type) {
    case BOOKMARKS:
      return true;
    default:
      return false;
  }
}

// static
void DeleteJournal::AddEntryToJournalIndex(JournalIndex* journal_index,
                                           std::unique_ptr<EntryKernel> entry) {
  EntryKernel* key = entry.get();
  if (journal_index->find(key) == journal_index->end())
    (*journal_index)[key] = std::move(entry);
}

}  // namespace syncable
}  // namespace syncer
