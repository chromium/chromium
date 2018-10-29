// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/syncable_write_transaction.h"

#include <string>

#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/directory_change_delegate.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/transaction_observer.h"
#include "components/sync/syncable/write_transaction_info.h"

namespace syncer {
namespace syncable {

const int64_t kInvalidTransactionVersion = -1;

WriteTransaction::WriteTransaction(const base::Location& location,
                                   WriterTag writer,
                                   Directory* directory)
    : BaseWriteTransaction(location, "WriteTransaction", writer, directory),
      transaction_version_(nullptr) {
  Lock();
}

WriteTransaction::WriteTransaction(const base::Location& location,
                                   Directory* directory,
                                   int64_t* transaction_version)
    : BaseWriteTransaction(location, "WriteTransaction", SYNCAPI, directory),
      transaction_version_(transaction_version) {
  Lock();
  if (transaction_version_)
    *transaction_version_ = kInvalidTransactionVersion;
}

void WriteTransaction::TrackChangesTo(const EntryKernel* entry) {
  if (!entry) {
    return;
  }
  // Insert only if it's not already there.
  const int64_t handle = entry->ref(META_HANDLE);
  auto it = mutations_.lower_bound(handle);
  if (it == mutations_.end() || it->first != handle) {
    mutations_[handle].original = *entry;
  }
}

ImmutableEntryKernelMutationMap WriteTransaction::RecordMutations() {
  directory_->kernel()->transaction_mutex.AssertAcquired();
  for (auto it = mutations_.begin(); it != mutations_.end();) {
    EntryKernel* kernel = directory()->GetEntryByHandle(it->first);
    if (!kernel) {
      NOTREACHED();
      continue;
    }
    if (kernel->is_dirty()) {
      it->second.mutated = *kernel;
      ++it;
    } else {
      DCHECK(!it->second.original.is_dirty());
      // Not actually mutated, so erase from |mutations_|.
      mutations_.erase(it++);
    }
  }
  return ImmutableEntryKernelMutationMap(&mutations_);
}

void WriteTransaction::UnlockAndNotify(
    const ImmutableEntryKernelMutationMap& mutations) {
  // Work while transaction mutex is held.
  ModelTypeSet models_with_changes;
  bool has_mutations = !mutations.Get().empty();
  if (has_mutations) {
    models_with_changes = NotifyTransactionChangingAndEnding(mutations);
  }
  Unlock();

  // Work after mutex is relased.
  if (has_mutations) {
    NotifyTransactionComplete(models_with_changes);
  }
}

ModelTypeSet WriteTransaction::NotifyTransactionChangingAndEnding(
    const ImmutableEntryKernelMutationMap& mutations) {
  directory_->kernel()->transaction_mutex.AssertAcquired();
  DCHECK(!mutations.Get().empty());

  WriteTransactionInfo write_transaction_info(
      directory_->kernel()->next_write_transaction_id, from_here_, writer_,
      mutations);
  ++directory_->kernel()->next_write_transaction_id;

  ImmutableWriteTransactionInfo immutable_write_transaction_info(
      &write_transaction_info);
  DirectoryChangeDelegate* const delegate = directory_->kernel()->delegate;
  std::vector<int64_t> entry_changed;
  if (writer_ == syncable::SYNCAPI) {
    delegate->HandleCalculateChangesChangeEventFromSyncApi(
        immutable_write_transaction_info, this, &entry_changed);
  } else {
    delegate->HandleCalculateChangesChangeEventFromSyncer(
        immutable_write_transaction_info, this, &entry_changed);
  }
  UpdateTransactionVersion(entry_changed);

  ModelTypeSet models_with_changes =
      delegate->HandleTransactionEndingChangeEvent(
          immutable_write_transaction_info, this);

  directory_->kernel()->transaction_observer.Call(
      FROM_HERE, &TransactionObserver::OnTransactionWrite,
      immutable_write_transaction_info, models_with_changes);

  return models_with_changes;
}

void WriteTransaction::NotifyTransactionComplete(
    ModelTypeSet models_with_changes) {
  directory_->kernel()->delegate->HandleTransactionCompleteChangeEvent(
      models_with_changes);
}

void WriteTransaction::UpdateTransactionVersion(
    const std::vector<int64_t>& entry_changed) {
  ModelTypeSet type_seen;
  for (uint32_t i = 0; i < entry_changed.size(); ++i) {
    MutableEntry entry(this, GET_BY_HANDLE, entry_changed[i]);
    if (entry.good()) {
      ModelType type = GetModelTypeFromSpecifics(entry.GetSpecifics());
      if (type < FIRST_REAL_MODEL_TYPE)
        continue;
      if (!type_seen.Has(type)) {
        directory_->IncrementTransactionVersion(type);
        type_seen.Put(type);
      }
      entry.UpdateTransactionVersion(directory_->GetTransactionVersion(type));
    }
  }

  if (!type_seen.Empty() && transaction_version_) {
    DCHECK_EQ(1u, type_seen.Size());
    *transaction_version_ =
        directory_->GetTransactionVersion(*(type_seen.begin()));
  }
}

WriteTransaction::~WriteTransaction() {
  const ImmutableEntryKernelMutationMap& mutations = RecordMutations();

  MetahandleSet modified_handles;
  for (auto i = mutations.Get().begin(); i != mutations.Get().end(); ++i) {
    modified_handles.insert(i->first);
  }
  directory()->CheckInvariantsOnTransactionClose(this, modified_handles);

  // |CheckTreeInvariants| could have thrown an unrecoverable error.
  if (unrecoverable_error_set_) {
    HandleUnrecoverableErrorIfSet();
    Unlock();
    return;
  }

  UnlockAndNotify(mutations);
}

#define ENUM_CASE(x) \
  case x:            \
    return #x;       \
    break

std::string WriterTagToString(WriterTag writer_tag) {
  switch (writer_tag) {
    ENUM_CASE(INVALID);
    ENUM_CASE(SYNCER);
    ENUM_CASE(AUTHWATCHER);
    ENUM_CASE(UNITTEST);
    ENUM_CASE(VACUUM_AFTER_SAVE);
    ENUM_CASE(HANDLE_SAVE_FAILURE);
    ENUM_CASE(PURGE_ENTRIES);
    ENUM_CASE(SYNCAPI);
  }
  NOTREACHED();
  return std::string();
}

#undef ENUM_CASE

}  // namespace syncable
}  // namespace syncer
