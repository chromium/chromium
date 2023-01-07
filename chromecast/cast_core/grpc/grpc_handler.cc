// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/grpc_handler.h"

#include "base/check.h"

namespace cast {
namespace utils {

GrpcHandler::GrpcHandler(ServerReactorTracker* server_reactor_tracker)
    : server_reactor_tracker_(server_reactor_tracker) {
  DCHECK(server_reactor_tracker_);
}

GrpcHandler::~GrpcHandler() = default;

}  // namespace utils
}  // namespace cast
