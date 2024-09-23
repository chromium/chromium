// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/server_reactor_tracker.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"

namespace cast {
namespace utils {

ServerReactorTracker::ServerReactorTracker() = default;

ServerReactorTracker::~ServerReactorTracker() {
  std::vector<grpc::ServerGenericBidiReactor*> reactors_to_destroy;
  {
    base::AutoLock l(lock_);
    if (active_reactors_.empty()) {
      return;
    }
    // Need to copy all active reactors to allow for Reactor::dtor() ->
    // RemoveReactor flow to happen without deadlocking.
    base::ranges::for_each(active_reactors_, [&](const auto& pair) {
      reactors_to_destroy.push_back(pair.first);
    });
  }

  // Force delete any pending server reactors to avoid memory leaks. The
  // handlers must confirm in their callbacks that the GrpcServer is available
  // before accessing the |reactor| in the callback.
  LOG(WARNING)
      << "Reactor tracker detected active reactors on destruction: count="
      << reactors_to_destroy.size();
  for (auto* reactor : reactors_to_destroy) {
    delete reactor;
  }
}

void ServerReactorTracker::AddReactor(grpc::ServerGenericBidiReactor* reactor,
                                      const std::string& name) {
  base::AutoLock l(lock_);
  active_reactors_.emplace(reactor, name);
}

void ServerReactorTracker::RemoveReactor(
    grpc::ServerGenericBidiReactor* reactor) {
  base::AutoLock l(lock_);
  active_reactors_.erase(reactor);
}

size_t ServerReactorTracker::active_reactor_count() const {
  base::AutoLock l(lock_);
  return active_reactors_.size();
}

}  // namespace utils
}  // namespace cast
