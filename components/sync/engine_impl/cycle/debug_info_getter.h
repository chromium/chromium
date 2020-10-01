// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DEBUG_INFO_GETTER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DEBUG_INFO_GETTER_H_

#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// This is the interface that needs to be implemented by the event listener
// to communicate the debug info data to the syncer.
class DebugInfoGetter {
 public:
  // Gets the client debug info. Be sure to clear the info to ensure the data
  // isn't sent multiple times.
  virtual sync_pb::DebugInfo GetDebugInfo() const = 0;

  // Clears the debug info.
  virtual void ClearDebugInfo() = 0;

  virtual ~DebugInfoGetter() {}
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DEBUG_INFO_GETTER_H_
