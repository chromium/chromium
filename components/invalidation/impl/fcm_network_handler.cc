// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_network_handler.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/status.h"
#include "components/invalidation/public/invalidator_state.h"

using instance_id::InstanceID;

namespace invalidation {

namespace {

const char kPayloadKey[] = "payload";
const char kPublicTopic[] = "external_name";
const char kVersionKey[] = "version";

// OAuth2 Scope passed to getToken to obtain GCM registration tokens.
// Must match Java GoogleCloudMessaging.INSTANCE_ID_SCOPE.
const char kGCMScope[] = "GCM";

// Lower bound time between two token validations when listening.
const int kTokenValidationPeriodMinutesDefault = 60 * 24;

// Returns the TTL (time-to-live) for the Instance ID token, or 0 if no TTL
// should be specified.
base::TimeDelta GetTimeToLive(const std::string& sender_id) {
  // This magic value is identical to kPolicyFCMInvalidationSenderID, i.e. the
  // value that ChromeOS policy uses for its invalidations.
  if (sender_id == "1013309121859") {
    if (!base::FeatureList::IsEnabled(switches::kPolicyInstanceIDTokenTTL)) {
      return base::TimeDelta();
    }

    return base::Seconds(switches::kPolicyInstanceIDTokenTTLSeconds.Get());
  }

  // The default for all other FCM clients is no TTL.
  return base::TimeDelta();
}

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
std::string UnpackPrivateTopic(std::string_view private_topic) {
  if (base::StartsWith(private_topic, "/topics/private/")) {
    return std::string(private_topic.substr(strlen("/topics")));
  } else if (base::StartsWith(private_topic, "/topics/")) {
    return std::string(private_topic.substr(strlen("/topics/")));
  } else {
    return std::string(private_topic);
  }
}

InvalidationParsingStatus ParseIncomingMessage(
    const gcm::IncomingMessage& message,
    std::string* payload,
    std::string* private_topic,
    std::string* public_topic,
    int64_t* version) {
  *payload = GetValueFromMessage(message, kPayloadKey);
  std::string version_str = GetValueFromMessage(message, kVersionKey);

  // Version must always be there, and be an integer.
  if (version_str.empty())
    return InvalidationParsingStatus::kVersionEmpty;
  if (!base::StringToInt64(version_str, version))
    return InvalidationParsingStatus::kVersionInvalid;

  *public_topic = GetValueFromMessage(message, kPublicTopic);

  *private_topic = UnpackPrivateTopic(message.sender_id);
  if (private_topic->empty())
    return InvalidationParsingStatus::kPrivateTopicEmpty;

  return InvalidationParsingStatus::kSuccess;
}

void RecordFCMMessageStatus(InvalidationParsingStatus status,
                            const std::string& sender_id) {
  // These histograms are recorded quite frequently, so use the macros rather
  // than the functions.
  UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.FCMMessageStatus", status);
  // Also split the histogram by a few well-known senders. The actual constants
  // aren't accessible here (they're defined in higher layers), so we simply
  // duplicate them here, strictly only for the purpose of metrics.
  constexpr char kDriveFcmSenderId[] = "947318989803";
  constexpr char kPolicyFCMInvalidationSenderID[] = "1013309121859";
  if (sender_id == kDriveFcmSenderId) {
    UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.FCMMessageStatus.Drive",
                              status);
  } else if (sender_id == kPolicyFCMInvalidationSenderID) {
    UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.FCMMessageStatus.Policy",
                              status);
  }
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
std::unique_ptr<FCMNetworkHandler> FCMNetworkHandler::Create(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    const std::string& sender_id,
    const std::string& app_id) {
  return std::make_unique<FCMNetworkHandler>(gcm_driver, instance_id_driver,
                                             sender_id, app_id);
}

void FCMNetworkHandler::StartListening() {
  if (IsListening()) {
    StopListening();
  }
  // Adding ourselves as Handler means start listening.
  // Being the listener is pre-requirement for token operations.
  gcm_driver_->AddAppHandler(app_id_, this);

  instance_id_driver_->GetInstanceID(app_id_)->GetToken(
      sender_id_, kGCMScope, GetTimeToLive(sender_id_),
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
  base::UmaHistogramEnumeration("FCMInvalidations.InitialTokenRetrievalStatus",
                                result);
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
      FROM_HERE, base::Minutes(kTokenValidationPeriodMinutesDefault),
      base::BindOnce(&FCMNetworkHandler::StartTokenValidation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FCMNetworkHandler::StartTokenValidation() {
  DCHECK(IsListening());

  instance_id_driver_->GetInstanceID(app_id_)->GetToken(
      sender_id_, kGCMScope, GetTimeToLive(sender_id_),
      /*flags=*/{InstanceID::Flags::kIsLazy},
      base::BindOnce(&FCMNetworkHandler::DidReceiveTokenForValidation,
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

  if (result == InstanceID::SUCCESS) {
    UpdateChannelState(FcmChannelState::ENABLED);
    if (token_ != new_token) {
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
  int64_t version = 0;
  InvalidationParsingStatus status = ParseIncomingMessage(
      message, &payload, &private_topic, &public_topic, &version);

  RecordFCMMessageStatus(status, sender_id_);

  if (status == InvalidationParsingStatus::kSuccess)
    DeliverIncomingMessage(payload, private_topic, public_topic, version);
}

void FCMNetworkHandler::OnMessagesDeleted(const std::string& app_id) {
  DCHECK_EQ(app_id, app_id_);
  // Note: As of 2020-02, this doesn't actually happen in practice.
}

void FCMNetworkHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED_IN_MIGRATION() << "FCMNetworkHandler doesn't send GCM messages.";
}

void FCMNetworkHandler::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED_IN_MIGRATION() << "FCMNetworkHandler doesn't send GCM messages.";
}

void FCMNetworkHandler::SetTokenValidationTimerForTesting(
    std::unique_ptr<base::OneShotTimer> token_validation_timer) {
  token_validation_timer_ = std::move(token_validation_timer);
}

}  // namespace invalidation
