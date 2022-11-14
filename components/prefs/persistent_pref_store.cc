// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/persistent_pref_store.h"
#include "base/task/sequenced_task_runner.h"

#include <utility>

void PersistentPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  // Default behavior for PersistentPrefStore implementation that don't issue
  // disk operations: schedule the callback immediately.
  // |synchronous_done_callback| is allowed to be invoked synchronously (and
  // must be here since we have no other way to post it which isn't the current
  // sequence).

  if (synchronous_done_callback)
    std::move(synchronous_done_callback).Run();

  if (reply_callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(reply_callback));
  }
}

bool PersistentPrefStore::IsInMemoryPrefStore() const {
  return false;
}
