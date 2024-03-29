// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_TRACKABLE_REACTOR_H_
#define CHROMECAST_CAST_CORE_GRPC_TRACKABLE_REACTOR_H_

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chromecast/cast_core/grpc/server_reactor_tracker.h"

namespace cast {
namespace utils {

// A facade around reactor implementation that allows to easily track active
// reactors in a given ServerReactorTracker.
template <typename TReactor>
class TrackableReactor : public TReactor {
 public:
  template <typename... TArgs>
  explicit TrackableReactor(ServerReactorTracker* server_reactor_tracker,
                            TArgs&&... args)
      : TReactor(std::forward<TArgs&&>(args)...),
        server_reactor_tracker_(server_reactor_tracker) {
    DCHECK(server_reactor_tracker_);
    server_reactor_tracker_->AddReactor(this, TReactor::name());
  }

  ~TrackableReactor() override { server_reactor_tracker_->RemoveReactor(this); }

 private:
  raw_ptr<ServerReactorTracker> const server_reactor_tracker_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_TRACKABLE_REACTOR_H_
