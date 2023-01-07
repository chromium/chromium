// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_DEBUG_INFO_GETTER_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_DEBUG_INFO_GETTER_H_

namespace sync_pb {
class DebugInfo;
}

namespace syncer {

// This is the interface that needs to be implemented by the event listener
// to communicate the debug info data to the syncer.
class DebugInfoGetter {
 public:
  virtual ~DebugInfoGetter() = default;

  // Gets the client debug info. Be sure to clear the info to ensure the data
  // isn't sent multiple times.
  virtual sync_pb::DebugInfo GetDebugInfo() const = 0;

  // Clears the debug info.
  virtual void ClearDebugInfo() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_DEBUG_INFO_GETTER_H_
