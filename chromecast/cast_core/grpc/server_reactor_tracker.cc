// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/server_reactor_tracker.h"

#include <ostream>

namespace cast {
namespace utils {

ServerReactorTracker::ServerReactorTracker() = default;

ServerReactorTracker::~ServerReactorTracker() = default;

std::ostream& operator<<(std::ostream& os,
                         const ServerReactorTracker& tracker) {
  base::AutoLock l(tracker.lock_);
  for (const auto& entry : tracker.active_reactors_) {
    os << entry.second << " ";
  }
  return os;
}

}  // namespace utils
}  // namespace cast
