// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_network_handler.h"

#include <memory>
#include <string>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/status.h"
#include "components/invalidation/public/invalidator_state.h"

using instance_id::InstanceID;

namespace syncer {

namespace {

const char kPayloadKey[] = "payload";
const char kPublicTopic[] = "external_name";
const char kVersionKey[] = "version";

// OAuth2 Scope passed to getToken to obtain GCM registration tokens.
// Must match Java GoogleCloudMessaging.INSTANCE_ID_SCOPE.
const char kGCMScope[] = "GCM";

// Lower bound time between two token validations when listening.
const int kTokenValidationPeriodMinutesDefault = 60 * 24;

std::string GetValueFromMessage(const gcm::IncomingMessage& message,
                                const std::string& key) {
  std::string value;
  auto it = message.data.find(key);
  if (it != message.data.end())
    value = it->second;
  return value;
}

// Unpacks the private topic included in messages to the form returned for
// subscription requests.
//
// Subscriptions for private topics generate a private topic from the public
// topic of the form "/private/${public_topic}-${something}. Messages include
// this as the sender in the form
// "/topics/private/${public_topic}-${something}". For such messages, strip the
// "/topics" prefix.
//
// Subscriptions for public topics pass-through the public topic unchanged:
// "${public_topic}". Messages include the sender in the form
// "/topics/${public_topic}". For these messages, strip the "/topics/" prefix.
//
// If the provided sender does not match either pattern, return it unchanged.
std::string UnpackPrivateTopic(base::StringPiece private_topic) {
  if (private_topic.starts_with("/topics/private/")) {
    return private_topic.substr(strlen("/topics")).as_string();
  } else if (private_topic.starts_with("/topics/")) {
    return private_topic.substr(strlen("/topics/")).as_string();
  } else {
    return private_topic.as_string();
  }
}

InvalidationParsingStatus ParseIncommingMessage(
    const gcm::IncomingMessage& message,
    std::string* payload,
    std::string* private_topic,
    std::string* public_topic,
    std::string* version) {
  *payload = GetValueFromMessage(message, kPayloadKey);
  *version = GetValueFromMessage(message, kVersionKey);

  // Version must always be there.
  if (version->empty())
    return InvalidationParsingStatus::kVersionEmpty;

  *public_topic = GetValueFromMessage(message, kPublicTopic);

  *private_topic = UnpackPrivateTopic(message.sender_id);
  if (private_topic->empty())
    return InvalidationParsingStatus::kPrivateTopicEmpty;

  return InvalidationParsingStatus::kSuccess;
}

}  // namespace

FCMNetworkHandler::FCMNetworkHandler(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    const std::string& sender_id,
    const std::string& app_id)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      token_validation_timer_(std::make_unique<base::OneShotTimer>()),
      sender_id_(sender_id),
      app_id_(app_id) {}

FCMNetworkHandler::~FCMNetworkHandler() {
  StopListening();
}

// static
std::unique_ptr<syncer::FCMNetworkHandler> FCMNetworkHandler::Create(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    const std::string& sender_id,
    const std::string& app_id) {
  return std::make_unique<syncer::FCMNetworkHandler>(
      gcm_driver, instance_id_driver, sender_id, app_id);
}

void FCMNetworkHandler::StartListening() {
  if (IsListening()) {
    StopListening();
  }
  // Adding ourselves as Handler means start listening.
  // Being the listener is pre-requirement for token operations.
  gcm_driver_->AddAppHandler(app_id_, this);

  diagnostic_info_.instance_id_token_requested = base::Time::Now();
  instance_id_driver_->GetInstanceID(app_id_)->GetToken(
      sender_id_, kGCMScope,
      /*options=*/std::map<std::string, std::string>(),
      /*flags=*/{InstanceID::Flags::kIsLazy},
      base::BindRepeating(&FCMNetworkHandler::DidRetrieveToken,
                          weak_ptr_factory_.GetWeakPtr()));
}

void FCMNetworkHandler::StopListening() {
  if (IsListening())
    gcm_driver_->RemoveAppHandler(app_id_);
}

bool FCMNetworkHandler::IsListening() const {
  return gcm_driver_->GetAppHandler(app_id_);
}

void FCMNetworkHandler::DidRetrieveToken(const std::string& subscription_token,
                                         InstanceID::Result result) {
  UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.InitialTokenRetrievalStatus",
                            result, InstanceID::Result::LAST_RESULT + 1);
  diagnostic_info_.registration_result = result;
  diagnostic_info_.token = subscription_token;
  diagnostic_info_.instance_id_token_was_received = base::Time::Now();
  switch (result) {
    case InstanceID::SUCCESS:
      // The received token is assumed to be valid, therefore, we reschedule
      // validation.
      DeliverToken(subscription_token);
      token_ = subscription_token;
      UpdateChannelState(FcmChannelState::ENABLED);
      break;
    case InstanceID::INVALID_PARAMETER:
    case InstanceID::DISABLED:
    case InstanceID::ASYNC_OPERATION_PENDING:
    case InstanceID::SERVER_ERROR:
    case InstanceID::UNKNOWN_ERROR:
    case InstanceID::NETWORK_ERROR:
      DLOG(WARNING) << "Messaging subscription failed; InstanceID::Result = "
                    << result;
      UpdateChannelState(FcmChannelState::NO_INSTANCE_ID_TOKEN);
      break;
  }
  ScheduleNextTokenValidation();
}

void FCMNetworkHandler::ScheduleNextTokenValidation() {
  DCHECK(IsListening());

  token_validation_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMinutes(kTokenValidationPeriodMinutesDefault),
      base::BindOnce(&FCMNetworkHandler::StartTokenValidation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FCMNetworkHandler::StartTokenValidation() {
  DCHECK(IsListening());

  diagnostic_info_.instance_id_token_verification_requested = base::Time::Now();
  diagnostic_info_.token_validation_requested_num++;
  instance_id_driver_->GetInstanceID(app_id_)->GetToken(
      sender_id_, kGCMScope, std::map<std::string, std::string>(),
      /*flags=*/{InstanceID::Flags::kIsLazy},
      base::Bind(&FCMNetworkHandler::DidReceiveTokenForValidation,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FCMNetworkHandler::DidReceiveTokenForValidation(
    const std::string& new_token,
    InstanceID::Result result) {
  if (!IsListening()) {
    // After we requested the token, |StopListening| has been called. Thus,
    // ignore the token.
    return;
  }

  diagnostic_info_.instance_id_token_verified = base::Time::Now();
  diagnostic_info_.token_verification_result = result;
  if (result == InstanceID::SUCCESS) {
    UpdateChannelState(FcmChannelState::ENABLED);
    if (token_ != new_token) {
      diagnostic_info_.token_changed = true;
      token_ = new_token;
      DeliverToken(new_token);
    }
  }

  ScheduleNextTokenValidation();
}

void FCMNetworkHandler::UpdateChannelState(FcmChannelState state) {
  if (channel_state_ == state)
    return;
  channel_state_ = state;
  NotifyChannelStateChange(channel_state_);
}

void FCMNetworkHandler::ShutdownHandler() {}

void FCMNetworkHandler::OnStoreReset() {}

void FCMNetworkHandler::OnMessage(const std::string& app_id,
                                  const gcm::IncomingMessage& message) {
  DCHECK_EQ(app_id, app_id_);
  std::string payload;
  std::string private_topic;
  std::string public_topic;
  std::string version;

  InvalidationParsingStatus status = ParseIncommingMessage(
      message, &payload, &private_topic, &public_topic, &version);
  UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.FCMMessageStatus", status);

  if (status == InvalidationParsingStatus::kSuccess)
    DeliverIncomingMessage(payload, private_topic, public_topic, version);
}

void FCMNetworkHandler::OnMessagesDeleted(const std::string& app_id) {
  // TODO(melandory): consider notifyint the client that messages were
  // deleted. So the client can act on it, e.g. in case of sync request
  // GetUpdates from the server.
}

void FCMNetworkHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED() << "FCMNetworkHandler doesn't send GCM messages.";
}

void FCMNetworkHandler::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED() << "FCMNetworkHandler doesn't send GCM messages.";
}

void FCMNetworkHandler::SetTokenValidationTimerForTesting(
    std::unique_ptr<base::OneShotTimer> token_validation_timer) {
  token_validation_timer_ = std::move(token_validation_timer);
}

void FCMNetworkHandler::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) {
  callback.Run(diagnostic_info_.CollectDebugData());
}

FCMNetworkHandlerDiagnostic::FCMNetworkHandlerDiagnostic() {}

base::DictionaryValue FCMNetworkHandlerDiagnostic::CollectDebugData() const {
  base::DictionaryValue status;
  status.SetString("NetworkHandler.Registration-result-code",
                   RegistrationResultToString(registration_result));
  status.SetString("NetworkHandler.Token", token);
  status.SetString(
      "NetworkHandler.Token-was-requested",
      base::TimeFormatShortDateAndTime(instance_id_token_requested));
  status.SetString(
      "NetworkHandler.Token-was-received",
      base::TimeFormatShortDateAndTime(instance_id_token_was_received));
  status.SetString("NetworkHandler.Token-verification-started",
                   base::TimeFormatShortDateAndTime(
                       instance_id_token_verification_requested));
  status.SetString(
      "NetworkHandler.Token-was-verified",
      base::TimeFormatShortDateAndTime(instance_id_token_verified));
  status.SetString("NetworkHandler.Verification-result-code",
                   RegistrationResultToString(token_verification_result));
  status.SetBoolean("NetworkHandler.Token-changed-when-verified",
                    token_changed);
  status.SetInteger("NetworkHandler.Token-validation-requests",
                    token_validation_requested_num);
  return status;
}

std::string FCMNetworkHandlerDiagnostic::RegistrationResultToString(
    const instance_id::InstanceID::Result result) const {
  switch (registration_result) {
    case instance_id::InstanceID::SUCCESS:
      return "InstanceID::SUCCESS";
    case instance_id::InstanceID::INVALID_PARAMETER:
      return "InstanceID::INVALID_PARAMETER";
    case instance_id::InstanceID::DISABLED:
      return "InstanceID::DISABLED";
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
      return "InstanceID::ASYNC_OPERATION_PENDING";
    case instance_id::InstanceID::SERVER_ERROR:
      return "InstanceID::SERVER_ERROR";
    case instance_id::InstanceID::UNKNOWN_ERROR:
      return "InstanceID::UNKNOWN_ERROR";
    case instance_id::InstanceID::NETWORK_ERROR:
      return "InstanceID::NETWORK_ERROR";
  }
}

}  // namespace syncer
