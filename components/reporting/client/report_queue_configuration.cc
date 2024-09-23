// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_configuration.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/types/expected.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/wrapped_rate_limiter.h"

namespace reporting {

ReportQueueConfiguration::Builder::Builder(
    const ReportQueueConfiguration::Settings& settings)
    : final_value_(base::WrapUnique<ReportQueueConfiguration>(
          new ReportQueueConfiguration())) {
  if (auto status = final_value_.value()->SetEventType(settings.event_type);
      !status.ok()) {
    final_value_ = base::unexpected(std::move(status));
    return;
  }
  if (auto status = final_value_.value()->SetDestination(settings.destination);
      !status.ok()) {
    final_value_ = base::unexpected(std::move(status));
    return;
  }
  if (settings.reserved_space != 0L) {
    if (auto status =
            final_value_.value()->SetReservedSpace(settings.reserved_space);
        !status.ok()) {
      final_value_ = base::unexpected(std::move(status));
      return;
    }
  }
}

ReportQueueConfiguration::Builder::Builder(
    ReportQueueConfiguration::Builder&& other) = default;

ReportQueueConfiguration::Builder::~Builder() = default;

ReportQueueConfiguration::Builder
ReportQueueConfiguration::Builder::SetPolicyCheckCallback(
    ReportQueueConfiguration::PolicyCheckCallback policy_check_callback) {
  if (final_value_.has_value()) {
    auto status =
        final_value_.value()->SetPolicyCheckCallback(policy_check_callback);
    if (!status.ok()) {
      final_value_ = base::unexpected(std::move(status));
    }
  }
  return std::move(*this);
}

ReportQueueConfiguration::Builder
ReportQueueConfiguration::Builder::SetRateLimiter(
    std::unique_ptr<RateLimiterInterface> rate_limiter) {
  if (final_value_.has_value()) {
    auto status = final_value_.value()->SetRateLimiter(std::move(rate_limiter));
    if (!status.ok()) {
      final_value_ = base::unexpected(std::move(status));
    }
  }
  return std::move(*this);
}

ReportQueueConfiguration::Builder ReportQueueConfiguration::Builder::SetDMToken(
    std::string_view dm_token) {
  if (final_value_.has_value()) {
    auto status = final_value_.value()->SetDMToken(dm_token);
    if (!status.ok()) {
      final_value_ = base::unexpected(std::move(status));
    }
  }
  return std::move(*this);
}

ReportQueueConfiguration::Builder
ReportQueueConfiguration::Builder::SetSourceInfo(
    std::optional<SourceInfo> source_info) {
  if (final_value_.has_value()) {
    auto status = final_value_.value()->SetSourceInfo(std::move(source_info));
    if (!status.ok()) {
      final_value_ = base::unexpected(std::move(status));
    }
  }
  return std::move(*this);
}

StatusOr<std::unique_ptr<ReportQueueConfiguration>>
ReportQueueConfiguration::Builder::Build() {
  auto result = std::move(final_value_);
  final_value_ = base::unexpected(
      Status(error::ALREADY_EXISTS, "Configuration has already been returned"));
  return result;
}

ReportQueueConfiguration::ReportQueueConfiguration() = default;
ReportQueueConfiguration::~ReportQueueConfiguration() = default;

// Factory for generating a ReportQueueConfiguration.
ReportQueueConfiguration::Builder ReportQueueConfiguration::Create(
    const ReportQueueConfiguration::Settings& settings) {
  return Builder(settings);
}


// static
StatusOr<std::unique_ptr<ReportQueueConfiguration>>
ReportQueueConfiguration::Create(
    std::string_view dm_token,
    Destination destination,
    PolicyCheckCallback policy_check_callback,
    std::unique_ptr<RateLimiterInterface> rate_limiter,
    int64_t reserved_space) {
  return ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                           .destination = destination,
                                           .reserved_space = reserved_space})
      .SetPolicyCheckCallback(policy_check_callback)
      .SetRateLimiter(std::move(rate_limiter))
      .SetDMToken(dm_token)
      .Build();
}

Status ReportQueueConfiguration::SetPolicyCheckCallback(
    PolicyCheckCallback policy_check_callback) {
  if (!policy_check_callback_.is_null()) {
    return Status(error::ALREADY_EXISTS, "PolicyCheckCallback cannot be reset");
  }
  if (policy_check_callback.is_null()) {
    return Status(error::INVALID_ARGUMENT,
                  "PolicyCheckCallback must not be null");
  }
  policy_check_callback_ = std::move(policy_check_callback);
  return Status::StatusOK();
}

Status ReportQueueConfiguration::SetEventType(EventType event_type) {
  event_type_ = event_type;
  return Status::StatusOK();
}

Status ReportQueueConfiguration::CheckPolicy() const {
  if (policy_check_callback_.is_null()) {
    return Status::StatusOK();
  }
  return policy_check_callback_.Run();
}

Status ReportQueueConfiguration::SetDMToken(std::string_view dm_token) {
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
  if (wrapped_rate_limiter_) {
    return Status(error::ALREADY_EXISTS, "RateLimiter cannot be reset");
  }
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

Status ReportQueueConfiguration::SetSourceInfo(
    std::optional<SourceInfo> source_info) {
  if (source_info_.has_value()) {
    return Status(error::ALREADY_EXISTS, "SourceInfo cannot be reset");
  }
  if (!source_info.has_value()) {
    // No source info specified. Also the default.
    source_info_ = std::nullopt;
    return Status::StatusOK();
  }

  // Validate if the specified source info has a source set and a valid one if
  // there is a version associated with it.
  const auto& source_info_value = source_info.value();
  if (!source_info_value.has_source() ||
      (source_info_value.source() == SourceInfo::SOURCE_UNSPECIFIED &&
       source_info_value.has_source_version())) {
    return Status(error::INVALID_ARGUMENT, "Must specify valid source");
  }
  source_info_ = std::move(source_info);
  return Status::StatusOK();
}
}  // namespace reporting
