// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_configuration.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/wrapped_rate_limiter.h"

namespace reporting {

ReportQueueConfiguration::ReportQueueConfiguration() = default;
ReportQueueConfiguration::~ReportQueueConfiguration() = default;

StatusOr<std::unique_ptr<ReportQueueConfiguration>>
ReportQueueConfiguration::Create(
    EventType event_type,
    Destination destination,
    PolicyCheckCallback policy_check_callback,
    std::unique_ptr<RateLimiterInterface> rate_limiter,
    int64_t reserved_space) {
  auto config = base::WrapUnique<ReportQueueConfiguration>(
      new ReportQueueConfiguration());

  RETURN_IF_ERROR(config->SetEventType(event_type));
  RETURN_IF_ERROR(config->SetDestination(destination));
  RETURN_IF_ERROR(config->SetPolicyCheckCallback(policy_check_callback));
  RETURN_IF_ERROR(config->SetRateLimiter(std::move(rate_limiter)));
  RETURN_IF_ERROR(config->SetReservedSpace(reserved_space));

  return config;
}

StatusOr<std::unique_ptr<ReportQueueConfiguration>>
ReportQueueConfiguration::Create(
    base::StringPiece dm_token,
    Destination destination,
    PolicyCheckCallback policy_check_callback,
    std::unique_ptr<RateLimiterInterface> rate_limiter,
    int64_t reserved_space) {
  auto config_result =
      Create(/*event_type=*/EventType::kDevice, destination,
             policy_check_callback, std::move(rate_limiter), reserved_space);
  if (!config_result.ok()) {
    return config_result;
  }

  std::unique_ptr<ReportQueueConfiguration> config =
      std::move(config_result.ValueOrDie());
  RETURN_IF_ERROR(config->SetDMToken(dm_token));

  return std::move(config);
}

Status ReportQueueConfiguration::SetPolicyCheckCallback(
    PolicyCheckCallback policy_check_callback) {
  if (policy_check_callback.is_null()) {
    return (Status(error::INVALID_ARGUMENT,
                   "PolicyCheckCallback must not be null"));
  }
  policy_check_callback_ = std::move(policy_check_callback);
  return Status::StatusOK();
}

Status ReportQueueConfiguration::SetEventType(EventType event_type) {
  event_type_ = event_type;
  return Status::StatusOK();
}

Status ReportQueueConfiguration::CheckPolicy() const {
  return policy_check_callback_.Run();
}

Status ReportQueueConfiguration::SetDMToken(base::StringPiece dm_token) {
  dm_token_ = std::string(dm_token);
  return Status::StatusOK();
}

Status ReportQueueConfiguration::SetDestination(Destination destination) {
  if (destination == Destination::UNDEFINED_DESTINATION) {
    return Status(error::INVALID_ARGUMENT, "Destination must be defined");
  }
  destination_ = destination;
  return Status::StatusOK();
}

Status ReportQueueConfiguration::SetRateLimiter(
    std::unique_ptr<RateLimiterInterface> rate_limiter) {
  if (rate_limiter) {
    wrapped_rate_limiter_ = WrappedRateLimiter::Create(std::move(rate_limiter));
    is_event_allowed_cb_ = wrapped_rate_limiter_->async_acquire_cb();
  }
  return Status::StatusOK();
}

Status ReportQueueConfiguration::SetReservedSpace(int64_t reserved_space) {
  if (reserved_space < 0L) {
    return Status(error::INVALID_ARGUMENT,
                  "Must reserve non-zero amount of space");
  }
  reserved_space_ = reserved_space;
  return Status::StatusOK();
}
}  // namespace reporting
