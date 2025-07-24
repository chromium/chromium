// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_invalidator.h"

#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/remote_commands/remote_commands_fetch_reason.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"

namespace policy {

namespace {

constexpr char kDeiceRemoteCommandsInvalidatorTypeName[] =
    "DEVICE_REMOTE_COMMAND";

constexpr char kBrowserRemoteCommandsInvalidatorTypeName[] =
    "BROWSER_REMOTE_COMMAND";

// Right now, consumer user remote commands are only supported on ChromeOS while
// profile remote commands are only supported on other platforms.
// If we want support support both commands on the same platform, we need to
// update the `PolicyInvalidationScope` to be more granular.
constexpr char kUserRemoteCommandsInvalidatorTypeName[] =
#if BUILDFLAG(IS_CHROMEOS)
    "CONSUMER_USER_REMOTE_COMMAND";
#else
    "PROFILE_REMOTE_COMMAND";
#endif

const char* GetInvalidationMetricName(PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserRemoteCommandInvalidations;
    case PolicyInvalidationScope::kDevice:
      return kMetricDeviceRemoteCommandInvalidations;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMRemoteCommandInvalidations;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED() << "Unexpected instance of remote commands invalidator with "
                      "device local account scope.";
  }
}

}  // namespace

RemoteCommandsInvalidator::RemoteCommandsInvalidator(
    invalidation::InvalidationListener* invalidation_listener,
    CloudPolicyCore* core,
    const base::Clock* clock,
    PolicyInvalidationScope scope)
    : scope_(scope),
      invalidation_listener_(invalidation_listener),
      core_(core),
      clock_(clock) {
  CHECK(invalidation_listener_);
  CHECK(core_);
  CHECK(clock_);

  core_observation_.Observe(core_);
  if (core_->remote_commands_service()) {
    OnRemoteCommandsServiceStarted(core_);
  }
}

RemoteCommandsInvalidator::~RemoteCommandsInvalidator() {
  // Explicitly reset observation of `InvalidationListener` as it needs
  // `GetType()` to remove observer and `GetType()` requires access to our
  // state.
  invalidation_listener_observation_.Reset();
}

void RemoteCommandsInvalidator::OnCoreConnected(CloudPolicyCore* core) {}

void RemoteCommandsInvalidator::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {}

void RemoteCommandsInvalidator::OnCoreDisconnecting(CloudPolicyCore* core) {
  invalidation_listener_observation_.Reset();
}

void RemoteCommandsInvalidator::OnRemoteCommandsServiceStarted(
    CloudPolicyCore* core) {
  invalidation_listener_observation_.Observe(invalidation_listener_);
}

void RemoteCommandsInvalidator::OnExpectationChanged(
    invalidation::InvalidationsExpected expected) {
  if (expected == invalidation::InvalidationsExpected::kYes &&
      are_invalidations_expected_ != expected) {
    // If an invalidation is sent before invalidations are registered, it may be
    // lost (see crbug.com/430014807). Fetch remote commands to cover possibly
    // lost invalidation.
    DoInitialRemoteCommandsFetch();
  }

  are_invalidations_expected_ = expected;
}

void RemoteCommandsInvalidator::OnInvalidationReceived(
    const invalidation::DirectInvalidation& invalidation) {
  if (!AreInvalidationsEnabled()) {
    LOG_POLICY(WARNING, REMOTE_COMMANDS) << "Unexpected invalidation received.";
  }

  DoRemoteCommandsFetch(invalidation);
}

std::string RemoteCommandsInvalidator::GetType() const {
  switch (scope_) {
    case PolicyInvalidationScope::kUser:
      return kUserRemoteCommandsInvalidatorTypeName;
    case PolicyInvalidationScope::kDevice:
      return kDeiceRemoteCommandsInvalidatorTypeName;
    case PolicyInvalidationScope::kCBCM:
      return kBrowserRemoteCommandsInvalidatorTypeName;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED() << "Device local account commands are not supported.";
  }
}

bool RemoteCommandsInvalidator::IsRegistered() const {
  return invalidation_listener_observation_.IsObservingSource(
      invalidation_listener_);
}

bool RemoteCommandsInvalidator::AreInvalidationsEnabled() const {
  if (!IsRegistered()) {
    return false;
  }

  return are_invalidations_expected_ ==
         invalidation::InvalidationsExpected::kYes;
}

void RemoteCommandsInvalidator::DoRemoteCommandsFetch(
    const invalidation::DirectInvalidation& invalidation) {
  CHECK(core_->remote_commands_service());

  RecordInvalidationMetric(invalidation);

  core_->remote_commands_service()->FetchRemoteCommands(
      RemoteCommandsFetchReason::kInvalidation);
}

void RemoteCommandsInvalidator::DoInitialRemoteCommandsFetch() {
  CHECK(core_->remote_commands_service());

  core_->remote_commands_service()->FetchRemoteCommands(
      RemoteCommandsFetchReason::kStartup);
}

void RemoteCommandsInvalidator::RecordInvalidationMetric(
    const invalidation::DirectInvalidation& invalidation) const {
  const auto last_fetch_time = base::Time::FromMillisecondsSinceUnixEpoch(
      core_->store()->policy()->timestamp());
  const auto current_time = clock_->Now();
  const bool is_expired =
      IsInvalidationExpired(invalidation, last_fetch_time, current_time);
  const bool is_missing_payload = invalidation.payload().empty();

  base::UmaHistogramEnumeration(
      GetInvalidationMetricName(scope_),
      GetInvalidationMetric(is_missing_payload, is_expired),
      POLICY_INVALIDATION_TYPE_SIZE);
}

}  // namespace policy
