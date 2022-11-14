// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/grpc_call_options.h"

#include <grpcpp/grpcpp.h>

#include <limits>

#include "base/check_op.h"
#include "base/logging.h"

namespace cast {
namespace utils {

namespace {

// Default gRPC call deadline.
static const auto kDefaultCallDeadline = 60 * 1000;

}  // namespace

GrpcCallOptions::GrpcCallOptions() : deadline_ms_(kDefaultCallDeadline) {}

GrpcCallOptions::~GrpcCallOptions() = default;

void GrpcCallOptions::SetDeadline(int64_t deadline_ms) {
  DCHECK_GE(deadline_ms, 0);
  deadline_ms_ = std::move(deadline_ms);
}

void GrpcCallOptions::ApplyOptionsToContext(
    grpc::ClientContext* context) const {
  if (deadline_ms_ == 0 ||
      deadline_ms_ == std::numeric_limits<int64_t>::max()) {
    return;
  }

  context->set_deadline(ToGprTimespec(deadline_ms_));
  DVLOG(1) << "Call deadline is set to " << deadline_ms_;
}

// static
gpr_timespec GrpcCallOptions::ToGprTimespec(int64_t duration_ms) {
  DCHECK_GE(duration_ms, 0);
  if (duration_ms == 0 || duration_ms == std::numeric_limits<int64_t>::max()) {
    return gpr_inf_future(GPR_CLOCK_MONOTONIC);
  }

  gpr_timespec span;
  span.tv_sec = duration_ms / 1000;
  span.tv_nsec = static_cast<int32_t>((duration_ms % 1000) * 1000 * 1000);
  span.clock_type = GPR_TIMESPAN;
  return gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC), span);
}

}  // namespace utils
}  // namespace cast
