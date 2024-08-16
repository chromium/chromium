// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NUDGE_HANDLER_H_
#define COMPONENTS_SYNC_ENGINE_NUDGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/sync/base/data_type.h"

namespace syncer {

class NudgeHandler {
 public:
  NudgeHandler() = default;
  virtual ~NudgeHandler() = default;

  // Schedules initial sync for |type| and returns.
  virtual void NudgeForInitialDownload(DataType type) = 0;
  // Schedules a commit for |type| and returns.
  virtual void NudgeForCommit(DataType type) = 0;
  // This method is called whenever pending invalidations have been updated
  // (added or removed).
  virtual void SetHasPendingInvalidations(DataType type,
                                          bool has_pending_invalidations) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NUDGE_HANDLER_H_
