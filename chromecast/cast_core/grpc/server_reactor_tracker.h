// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_SERVER_REACTOR_TRACKER_H_
#define CHROMECAST_CAST_CORE_GRPC_SERVER_REACTOR_TRACKER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace cast {
namespace utils {

// Tracks number of currently active reactors per server and allows to quickly
// diagnose the rpc names of the reactors that are left pending via ostream
// operator.
class ServerReactorTracker final {
 public:
  ServerReactorTracker();
  ~ServerReactorTracker();

  template <typename TReactor>
  void AddReactor(TReactor* reactor) {
    base::AutoLock l(lock_);
    active_reactors_.emplace(static_cast<void*>(reactor), reactor->name());
  }

  template <typename TReactor>
  void RemoveReactor(TReactor* reactor) {
    base::AutoLock l(lock_);
    active_reactors_.erase(static_cast<void*>(reactor));
  }

  size_t active_reactor_count() {
    base::AutoLock l(lock_);
    return active_reactors_.size();
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const ServerReactorTracker& tracker);

 private:
  mutable base::Lock lock_;
  base::flat_map<void*, std::string> active_reactors_ GUARDED_BY(lock_);
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_SERVER_REACTOR_TRACKER_H_
