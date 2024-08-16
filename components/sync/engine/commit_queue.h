// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_QUEUE_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_QUEUE_H_

#include "components/sync/engine/commit_and_get_updates_types.h"

namespace syncer {

// Interface used by a synced data type to issue requests to the sync backend.
// The actual implementation (DataTypeWorker) lives on the sync sequence, but
// there's typically a proxy object on the model sequence for use by the
// processor.
class CommitQueue {
 public:
  CommitQueue() = default;
  virtual ~CommitQueue() = default;

  // Nudge sync engine to indicate the datatype has local changes to commit.
  virtual void NudgeForCommit() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_QUEUE_H_
