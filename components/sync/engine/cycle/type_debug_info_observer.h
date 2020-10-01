// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_TYPE_DEBUG_INFO_OBSERVER_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_TYPE_DEBUG_INFO_OBSERVER_H_

#include "components/sync/base/model_type.h"

namespace syncer {

struct CommitCounters;
struct StatusCounters;
struct UpdateCounters;

// Interface for classes that observe per-type sync debug counters.
class TypeDebugInfoObserver {
 public:
  TypeDebugInfoObserver() = default;
  virtual ~TypeDebugInfoObserver() = default;

  virtual void OnCommitCountersUpdated(ModelType type,
                                       const CommitCounters& counters) = 0;
  virtual void OnUpdateCountersUpdated(ModelType type,
                                       const UpdateCounters& counters) = 0;
  virtual void OnStatusCountersUpdated(ModelType type,
                                       const StatusCounters& counters) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_TYPE_DEBUG_INFO_OBSERVER_H_
