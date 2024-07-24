// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_invalidator_impl.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/clock.h"
#include "components/invalidation/public/invalidation.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"

namespace policy {

namespace {

const char* GetInvalidationMetricName(PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserRemoteCommandInvalidations;
    case PolicyInvalidationScope::kDevice:
      return kMetricDeviceRemoteCommandInvalidations;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMRemoteCommandInvalidations;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected instance of remote commands invalidator with "
             "device local account scope.";
      return "";
  }
}

std::string ComposeOwnerName(PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return "RemoteCommands.User";
    case PolicyInvalidationScope::kDevice:
      return "RemoteCommands.Device";
    case PolicyInvalidationScope::kCBCM:
      return "RemoteCommands.CBCM";
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected instance of remote commands invalidator with "
             "device local account scope.";
      return "";
  }
}

}  // namespace

RemoteCommandsInvalidatorImpl::RemoteCommandsInvalidatorImpl(
    CloudPolicyCore* core,
    const base::Clock* clock,
    PolicyInvalidationScope scope)
    : RemoteCommandsInvalidator(ComposeOwnerName(scope), scope),
      core_(core),
      clock_(clock),
      scope_(scope) {
  DCHECK(core_);
}

void RemoteCommandsInvalidatorImpl::OnInitialize() {
  core_->AddObserver(this);
  if (core_->remote_commands_service()) {
    OnRemoteCommandsServiceStarted(core_);
  }
}

void RemoteCommandsInvalidatorImpl::OnShutdown() {
  core_->RemoveObserver(this);
}

void RemoteCommandsInvalidatorImpl::OnStart() {
  core_->store()->AddObserver(this);
  OnStoreLoaded(core_->store());
}

void RemoteCommandsInvalidatorImpl::OnStop() {
  core_->store()->RemoveObserver(this);
}

void RemoteCommandsInvalidatorImpl::DoRemoteCommandsFetch(
    const invalidation::Invalidation& invalidation) {
  DCHECK(core_->remote_commands_service());

  RecordInvalidationMetric(invalidation);

  core_->remote_commands_service()->FetchRemoteCommands();
}

void RemoteCommandsInvalidatorImpl::DoInitialRemoteCommandsFetch() {
  CHECK(core_->remote_commands_service());

  core_->remote_commands_service()->FetchRemoteCommands();
}

void RemoteCommandsInvalidatorImpl::OnCoreConnected(CloudPolicyCore* core) {}

void RemoteCommandsInvalidatorImpl::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {}

void RemoteCommandsInvalidatorImpl::OnCoreDisconnecting(CloudPolicyCore* core) {
  Stop();
}

void RemoteCommandsInvalidatorImpl::OnRemoteCommandsServiceStarted(
    CloudPolicyCore* core) {
  Start();
}

void RemoteCommandsInvalidatorImpl::OnStoreLoaded(CloudPolicyStore* core) {
  ReloadPolicyData(core_->store()->policy());
}

void RemoteCommandsInvalidatorImpl::OnStoreError(CloudPolicyStore* core) {}

void RemoteCommandsInvalidatorImpl::RecordInvalidationMetric(
    const invalidation::Invalidation& invalidation) const {
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
