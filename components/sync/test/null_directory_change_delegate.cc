// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/null_directory_change_delegate.h"

namespace syncer {
namespace syncable {

NullDirectoryChangeDelegate::~NullDirectoryChangeDelegate() {}

void NullDirectoryChangeDelegate::HandleCalculateChangesChangeEventFromSyncApi(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    BaseTransaction* trans,
    std::vector<int64_t>* entries_changed) {
  for (auto it = write_transaction_info.Get().mutations.Get().begin();
       it != write_transaction_info.Get().mutations.Get().end(); ++it) {
    entries_changed->push_back(it->first);
  }
}

void NullDirectoryChangeDelegate::HandleCalculateChangesChangeEventFromSyncer(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    BaseTransaction* trans,
    std::vector<int64_t>* entries_changed) {
  for (auto it = write_transaction_info.Get().mutations.Get().begin();
       it != write_transaction_info.Get().mutations.Get().end(); ++it) {
    entries_changed->push_back(it->first);
  }
}

ModelTypeSet NullDirectoryChangeDelegate::HandleTransactionEndingChangeEvent(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    BaseTransaction* trans) {
  return ModelTypeSet();
}

void NullDirectoryChangeDelegate::HandleTransactionCompleteChangeEvent(
    ModelTypeSet models_with_changes) {}

}  // namespace syncable
}  // namespace syncer
