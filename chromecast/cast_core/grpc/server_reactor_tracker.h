// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_SERVER_REACTOR_TRACKER_H_
#define CHROMECAST_CAST_CORE_GRPC_SERVER_REACTOR_TRACKER_H_

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "chromecast/cast_core/grpc/grpc_server_reactor.h"

namespace cast {
namespace utils {

// Tracks number of currently active reactors per server and allows to quickly
// diagnose the rpc names of the reactors that are left pending via ostream
// operator.
class ServerReactorTracker final {
 public:
  ServerReactorTracker();
  ~ServerReactorTracker();

  void AddReactor(grpc::ServerGenericBidiReactor* reactor,
                  const std::string& name);
  void RemoveReactor(grpc::ServerGenericBidiReactor* reactor);

  size_t active_reactor_count() const;

 private:
  mutable base::Lock lock_;
  base::flat_map<grpc::ServerGenericBidiReactor*, std::string> active_reactors_
      GUARDED_BY(lock_);
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_SERVER_REACTOR_TRACKER_H_
