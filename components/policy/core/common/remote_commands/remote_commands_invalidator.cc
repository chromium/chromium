// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_invalidator.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/core/common/policy_logger.h"

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

}  // namespace

RemoteCommandsInvalidator::RemoteCommandsInvalidator(
    PolicyInvalidationScope scope)
    : scope_(scope) {}

RemoteCommandsInvalidator::~RemoteCommandsInvalidator() {
  CHECK_EQ(SHUT_DOWN, state_);
}

void RemoteCommandsInvalidator::Initialize(
    invalidation::InvalidationListener* invalidation_listener) {
  CHECK_EQ(SHUT_DOWN, state_);
  CHECK(invalidation_listener);
  CHECK(thread_checker_.CalledOnValidThread());
  invalidation_listener_ = invalidation_listener;

  state_ = STOPPED;

  OnInitialize();
}

void RemoteCommandsInvalidator::Shutdown() {
  CHECK_NE(SHUT_DOWN, state_);
  DCHECK(thread_checker_.CalledOnValidThread());

  Stop();

  invalidation_listener_ = nullptr;
  state_ = SHUT_DOWN;
  OnShutdown();
}

void RemoteCommandsInvalidator::Start() {
  CHECK_EQ(STOPPED, state_);
  DCHECK(thread_checker_.CalledOnValidThread());

  state_ = STARTED;

  invalidation_listener_observation_.Observe(invalidation_listener_);

  OnStart();
}

void RemoteCommandsInvalidator::Stop() {
  CHECK_NE(SHUT_DOWN, state_);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == STARTED) {
    invalidation_listener_observation_.Reset();
    state_ = STOPPED;
    OnStop();
  }
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
  return invalidation_listener_ &&
         invalidation_listener_observation_.IsObservingSource(
             invalidation_listener_);
}

bool RemoteCommandsInvalidator::AreInvalidationsEnabled() const {
  if (!IsRegistered()) {
    return false;
  }

  return are_invalidations_expected_ ==
         invalidation::InvalidationsExpected::kYes;
}

}  // namespace policy
