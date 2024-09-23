// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_invalidator.h"

#include <string>

#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/core/common/policy_logger.h"

namespace policy {

namespace {

constexpr char kDeiceRemoteCommandsInvalidatorTypeName[] =
    "DEVICE_REMOTE_COMMAND";
constexpr char kUserRemoteCommandsInvalidatorTypeName[] =
    "CONSUMER_USER_REMOTE_COMMAND";

}  // namespace

RemoteCommandsInvalidator::RemoteCommandsInvalidator(
    std::string owner_name,
    PolicyInvalidationScope scope)
    : owner_name_(std::move(owner_name)), scope_(scope) {}

RemoteCommandsInvalidator::~RemoteCommandsInvalidator() {
  CHECK_EQ(SHUT_DOWN, state_);
}

void RemoteCommandsInvalidator::Initialize(
    std::variant<invalidation::InvalidationService*,
                 invalidation::InvalidationListener*>
        invalidation_service_or_listener) {
  CHECK_EQ(SHUT_DOWN, state_);
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK(!std::holds_alternative<invalidation::InvalidationService*>(
            invalidation_service_or_listener) ||
        std::get<invalidation::InvalidationService*>(
            invalidation_service_or_listener))
      << "InvalidationService is used but is null";
  CHECK(!std::holds_alternative<invalidation::InvalidationListener*>(
            invalidation_service_or_listener) ||
        std::get<invalidation::InvalidationListener*>(
            invalidation_service_or_listener))
      << "InvalidationListener is used but is null";
  invalidation_service_or_listener_ = invalidation::PointerVariantToRawPointer(
      invalidation_service_or_listener);

  state_ = STOPPED;

  // TODO(crbug.com/40283068): Reset `invalidation_service_` to avoid dangling
  // pointer.
  OnInitialize();
}

void RemoteCommandsInvalidator::Shutdown() {
  CHECK_NE(SHUT_DOWN, state_);
  DCHECK(thread_checker_.CalledOnValidThread());

  Stop();

  std::visit([](auto& v) { v = nullptr; }, invalidation_service_or_listener_);

  state_ = SHUT_DOWN;
  OnShutdown();
}

void RemoteCommandsInvalidator::Start() {
  CHECK_EQ(STOPPED, state_);
  DCHECK(thread_checker_.CalledOnValidThread());

  state_ = STARTED;

  std::visit(base::Overloaded{
                 [](invalidation::InvalidationService* service) {
                   // Do nothing.
                 },
                 [this](invalidation::InvalidationListener* listener) {
                   invalidation_listener_observation_.Observe(listener);
                 },
             },
             invalidation_service_or_listener_);

  OnStart();
}

void RemoteCommandsInvalidator::Stop() {
  CHECK_NE(SHUT_DOWN, state_);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == STARTED) {
    invalidation_service_observation_.Reset();
    invalidation_listener_observation_.Reset();
    // Drop the reference to `invalidation_service_or_listener_` as it
    // may be destroyed sooner than `DeviceLocalAccountPolicyService`.

    state_ = STOPPED;

    OnStop();
  }
}

void RemoteCommandsInvalidator::OnInvalidatorStateChange(
    invalidation::InvalidatorState state) {
  CHECK_EQ(STARTED, state_);
  DCHECK(thread_checker_.CalledOnValidThread());
}

void RemoteCommandsInvalidator::OnIncomingInvalidation(
    const invalidation::Invalidation& invalidation) {
  CHECK_EQ(STARTED, state_);
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG_POLICY(2, REMOTE_COMMANDS) << "Received remote command invalidation";

  if (!AreInvalidationsEnabled()) {
    LOG_POLICY(WARNING, REMOTE_COMMANDS) << "Unexpected invalidation received.";
  }

  CHECK(invalidation.topic() == topic_);

  DoRemoteCommandsFetch(invalidation);
}

std::string RemoteCommandsInvalidator::GetOwnerName() const {
  return owner_name_;
}

bool RemoteCommandsInvalidator::IsPublicTopic(
    const invalidation::Topic& topic) const {
  return IsPublicInvalidationTopic(topic);
}

void RemoteCommandsInvalidator::OnSuccessfullySubscribed(
    const invalidation::Topic& invalidation) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CHECK(invalidation == topic_);

  // The service needs to be started to fetch commands.
  if (state_ != STARTED) {
    return;
  }

  VLOG_POLICY(2, REMOTE_COMMANDS)
      << "Fetching remote commands after subscribing to invalidations.";

  DoInitialRemoteCommandsFetch();
}

void RemoteCommandsInvalidator::OnExpectationChanged(
    invalidation::InvalidationsExpected expected) {
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
    case PolicyInvalidationScope::kCBCM:
      return kDeiceRemoteCommandsInvalidatorTypeName;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED() << "Device local account commands are not supported.";
  }
}

void RemoteCommandsInvalidator::ReloadPolicyData(
    const enterprise_management::PolicyData* policy) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != STARTED) {
    return;
  }

  std::visit(base::Overloaded{
                 [this, policy](invalidation::InvalidationService* service) {
                   ReloadPolicyDataWithInvalidationService(policy);
                 },
                 [this](invalidation::InvalidationListener* listener) {
                   // With `invalidation_service_`, invalidator does initial
                   // fetch after subscribing. Without service there's no
                   // subscription event. Let's do initial fetch right away.
                   DoInitialRemoteCommandsFetch();
                 },
             },
             invalidation_service_or_listener_);
}

void RemoteCommandsInvalidator::ReloadPolicyDataWithInvalidationService(
    const enterprise_management::PolicyData* policy) {
  // Create the Topic based on the policy data.
  // If the policy does not specify the Topic, then unsubscribe and
  // unregister.
  invalidation::Topic topic;
  if (!policy || !GetRemoteCommandTopicFromPolicy(*policy, &topic)) {
    UnsubscribeFromTopicsWithInvalidationService();
    invalidation_service_observation_.Reset();
    return;
  }

  // If the policy topic in the policy data is different from the
  // currently registered topic, update the object registration.
  if (!IsRegistered() || topic != topic_) {
    RegisterWithInvalidationService(topic);
  }
}

bool RemoteCommandsInvalidator::IsRegistered() const {
  return std::visit(
      base::Overloaded{
          [this](invalidation::InvalidationService* service) {
            return service &&
                   invalidation_service_observation_.IsObservingSource(service);
          },
          [this](invalidation::InvalidationListener* listener) {
            return listener &&
                   invalidation_listener_observation_.IsObservingSource(
                       listener);
          }},
      invalidation_service_or_listener_);
}

bool RemoteCommandsInvalidator::AreInvalidationsEnabled() const {
  if (!IsRegistered()) {
    return false;
  }

  return std::visit(
      base::Overloaded{[](invalidation::InvalidationService* service) {
                         return service->GetInvalidatorState() ==
                                invalidation::InvalidatorState::kEnabled;
                       },
                       [this](invalidation::InvalidationListener* listener) {
                         return are_invalidations_expected_ ==
                                invalidation::InvalidationsExpected::kYes;
                       }},
      invalidation_service_or_listener_);
}

void RemoteCommandsInvalidator::RegisterWithInvalidationService(
    const invalidation::Topic& topic) {
  CHECK(std::holds_alternative<raw_ptr<invalidation::InvalidationService>>(
      invalidation_service_or_listener_));
  auto invalidation_service =
      std::get<raw_ptr<invalidation::InvalidationService>>(
          invalidation_service_or_listener_);

  // Register this handler with the invalidation service if needed.
  if (!IsRegistered()) {
    OnInvalidatorStateChange(invalidation_service->GetInvalidatorState());
    invalidation_service_observation_.Observe(invalidation_service);
  }

  topic_ = topic;

  // Update subscription with the invalidation service.
  const bool success =
      invalidation_service->UpdateInterestedTopics(this, /*topics=*/{topic});
  CHECK(success) << "Could not subscribe to topic: " << topic;
}

void RemoteCommandsInvalidator::UnsubscribeFromTopicsWithInvalidationService() {
  CHECK(std::holds_alternative<raw_ptr<invalidation::InvalidationService>>(
      invalidation_service_or_listener_));
  auto invalidation_service =
      std::get<raw_ptr<invalidation::InvalidationService>>(
          invalidation_service_or_listener_);

  base::ScopedObservation<invalidation::InvalidationService,
                          invalidation::InvalidationHandler>
      temporary_registration(this);

  // Invalidator cannot unset its topics without being registered. Let's quickly
  // register and unregister to do just that.
  if (!IsRegistered()) {
    temporary_registration.Observe(invalidation_service);
  }

  CHECK(invalidation_service->UpdateInterestedTopics(this,
                                                     invalidation::TopicSet()));
}

invalidation::InvalidationService*
RemoteCommandsInvalidator::invalidation_service() {
  if (std::holds_alternative<raw_ptr<invalidation::InvalidationService>>(
          invalidation_service_or_listener_)) {
    return std::get<raw_ptr<invalidation::InvalidationService>>(
        invalidation_service_or_listener_);
  }
  return nullptr;
}

}  // namespace policy
