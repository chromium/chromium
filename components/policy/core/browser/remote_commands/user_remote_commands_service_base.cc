// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/remote_commands/user_remote_commands_service_base.h"

#include "base/time/default_clock.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_invalidator_impl.h"

namespace policy {

UserRemoteCommandsServiceBase::UserRemoteCommandsServiceBase(
    CloudPolicyCore* core)
    : core_(core) {}
UserRemoteCommandsServiceBase::~UserRemoteCommandsServiceBase() = default;

void UserRemoteCommandsServiceBase::Init() {
  CHECK(core_);
  if (!core_->service()->IsInitializationComplete()) {
    cloud_policy_service_observer_.Observe(core_->service());
    return;
  }

  OnCloudPolicyServiceInitializationCompleted();
}

void UserRemoteCommandsServiceBase::
    OnCloudPolicyServiceInitializationCompleted() {
  cloud_policy_service_observer_.Reset();
  CHECK(core_);
  auto* invalidation_provider = GetInvalidationProvider();
  if (!invalidation_provider) {
    return;
  }
  core_->StartRemoteCommandsService(GetFactory(),
                                    PolicyInvalidationScope::kUser);
  invalidator_ = std::make_unique<RemoteCommandsInvalidatorImpl>(
      core_, base::DefaultClock::GetInstance(), PolicyInvalidationScope::kUser);
  invalidator_->Initialize(
      invalidation_provider->GetInvalidationServiceOrListener(
          kPolicyFCMInvalidationSenderID,
          invalidation::InvalidationListener::kProjectNumberEnterprise));
}

void UserRemoteCommandsServiceBase::OnPolicyRefreshed(bool success) {}

void UserRemoteCommandsServiceBase::Shutdown() {
  cloud_policy_service_observer_.Reset();
  if (invalidator_) {
    invalidator_->Shutdown();
    // Reset `invalidator_` ahead of time to avoid dangling pointer from
    // `RemoteCommandsInvalidator` to `ProfileInvalidationProvider`.
    invalidator_.reset();
  }
}

std::string_view UserRemoteCommandsServiceBase::name() const {
  return "UserRemoteCommandsServiceBase";
}

}  // namespace policy
