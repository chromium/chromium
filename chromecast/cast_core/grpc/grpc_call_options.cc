// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/grpc_call_options.h"

#include <grpcpp/grpcpp.h>

#include "base/logging.h"
#include "base/time/time.h"

namespace cast {
namespace utils {

namespace {

// Default gRPC call deadline.
static const auto kDefaultCallDeadline = base::Seconds(60);

}  // namespace

GrpcCallOptions::GrpcCallOptions() : deadline_(kDefaultCallDeadline) {}

GrpcCallOptions::~GrpcCallOptions() = default;

void GrpcCallOptions::SetDeadline(base::TimeDelta deadline) {
  DCHECK_GE(deadline.InMicroseconds(), 0);
  deadline_ = std::move(deadline);
}

void GrpcCallOptions::ApplyOptionsToContext(
    grpc::ClientContext* context) const {
  if (!deadline_.is_zero() && !deadline_.is_inf()) {
    context->set_deadline(ToGprTimespec(deadline_));
    DVLOG(1) << "Call deadline is set to " << deadline_;
  }
}

// static
gpr_timespec GrpcCallOptions::ToGprTimespec(const base::TimeDelta& delta) {
  DCHECK_GE(delta.InMicroseconds(), 0);
  if (delta.is_inf() || delta.is_zero()) {
    return gpr_inf_future(GPR_CLOCK_MONOTONIC);
  }

  timespec ts = delta.ToTimeSpec();
  gpr_timespec span;
  span.tv_sec = ts.tv_sec;
  span.tv_nsec = static_cast<int32_t>(ts.tv_nsec);
  span.clock_type = GPR_TIMESPAN;
  return gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC), span);
}

}  // namespace utils
}  // namespace cast
