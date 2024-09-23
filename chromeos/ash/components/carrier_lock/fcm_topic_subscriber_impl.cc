// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/fcm_topic_subscriber_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"

namespace ash::carrier_lock {

FcmTopicSubscriberImpl::FcmTopicSubscriberImpl(
    gcm::GCMDriver* gcm_driver,
    std::string app_id,
    std::string sender_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : gcm_driver_(gcm_driver),
      url_loader_factory_(url_loader_factory),
      app_id_(app_id),
      sender_id_(sender_id),
      weakptr_factory_(this) {
  DCHECK(!app_id.empty());
  DCHECK(!sender_id.empty());
}

FcmTopicSubscriberImpl::~FcmTopicSubscriberImpl() {
  if (gcm_driver_) {
    gcm_driver_->RemoveAppHandler(app_id_);
  }
}

bool FcmTopicSubscriberImpl::Initialize(NotificationCallback notification) {
  if (!gcm_driver_) {
    return false;
  }
  notification_callback_ = std::move(notification);
  if (!gcm_driver_->GetAppHandler(app_id_)) {
    gcm_driver_->AddAppHandler(app_id_, this);
  }
  if (!instance_id_driver_) {
    instance_id_driver_ =
        std::make_unique<instance_id::InstanceIDDriver>(gcm_driver_);
  }
  if (instance_id_driver_) {
    return true;
  }
  return false;
}

void FcmTopicSubscriberImpl::SubscribeTopic(const std::string& topic,
                                            Callback callback) {
  if (request_callback_) {
    LOG(ERROR) << "FcmTopicSubscriberImpl cannot handle multiple requests.";
    std::move(callback).Run(Result::kHandlerBusy);
    return;
  }

  topic_ = topic;
  subscribe_callback_ = std::move(callback);

  if (token_.empty()) {
    RequestToken(base::BindOnce(&FcmTopicSubscriberImpl::Subscribe,
                                weakptr_factory_.GetWeakPtr()));
  } else {
    Subscribe(Result::kSuccess);
  }
}

void FcmTopicSubscriberImpl::RequestToken(Callback callback) {
  if (request_callback_) {
    LOG(ERROR) << "FcmTopicSubscriberImpl cannot handle multiple requests.";
    std::move(callback).Run(Result::kHandlerBusy);
    return;
  }

  request_callback_ = std::move(callback);

  if (!instance_id_driver_) {
    LOG(ERROR) << "No FCM instance id driver!";
    ReturnError(Result::kInitializationFailed);
    return;
  }

  instance_id::InstanceID* iid = instance_id_driver_->GetInstanceID(app_id_);
  if (!iid) {
    LOG(ERROR) << "Failed to get FCM Instance ID";
    ReturnError(Result::kInitializationFailed);
    return;
  }

  iid->GetToken(sender_id_, instance_id::kGCMScope,
                /*time_to_live=*/base::TimeDelta(),
                {instance_id::InstanceID::Flags::kIsLazy},
                base::BindOnce(&FcmTopicSubscriberImpl::OnGetToken,
                               weakptr_factory_.GetWeakPtr()));
}

std::string const FcmTopicSubscriberImpl::token() {
  return token_;
}

void FcmTopicSubscriberImpl::OnGetToken(
    const std::string& token,
    instance_id::InstanceID::Result result) {
  if (result == instance_id::InstanceID::Result::SUCCESS) {
    token_ = token;
    if (is_testing()) {
      // The unit tests use FakeGCMDriverForInstanceID which does not implement
      // GetGCMStatistics method and the call below times out. Returning success
      // here is needed to continue with the values defined in unit test.
      ReturnSuccess();
      return;
    }
    gcm_driver_->GetGCMStatistics(
        base::BindOnce(&FcmTopicSubscriberImpl::OnGetGcmStatistics,
                       weakptr_factory_.GetWeakPtr()),
        gcm::GCMDriver::KEEP_LOGS);
    return;
  }

  LOG(ERROR) << "Failed to get FCM token";
  switch (result) {
    case instance_id::InstanceID::INVALID_PARAMETER:
      ReturnError(Result::kInvalidInput);
      return;
    case instance_id::InstanceID::DISABLED:
      ReturnError(Result::kInitializationFailed);
      return;
    case instance_id::InstanceID::NETWORK_ERROR:
      ReturnError(Result::kConnectionError);
      return;
    case instance_id::InstanceID::SERVER_ERROR:
      ReturnError(Result::kServerInternalError);
      return;
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
    case instance_id::InstanceID::UNKNOWN_ERROR:
    default:
      ReturnError(Result::kRequestFailed);
  }
}

void FcmTopicSubscriberImpl::OnGetGcmStatistics(
    const gcm::GCMClient::GCMStatistics& statistics) {
  android_id_ = statistics.android_id;
  android_secret_ = statistics.android_secret;
  ReturnSuccess();
}

void FcmTopicSubscriberImpl::Subscribe(Result request_token_result) {
  request_callback_ = std::move(subscribe_callback_);
  if (request_token_result != Result::kSuccess) {
    ReturnError(request_token_result);
    return;
  }
  if (topic_.empty() || !android_secret_) {
    ReturnError(Result::kInvalidInput);
    return;
  }

  TopicSubscriptionRequest::RequestInfo request_info(
      android_id_, android_secret_, app_id_, token_, topic_,
      /*unsubscribe=*/false);
  subscription_request_ = std::make_unique<TopicSubscriptionRequest>(
      request_info, url_loader_factory_, std::move(request_callback_));
  subscription_request_->Start();
}

void FcmTopicSubscriberImpl::ReturnError(Result err) {
  std::move(request_callback_).Run(err);
}

void FcmTopicSubscriberImpl::ReturnSuccess() {
  std::move(request_callback_).Run(Result::kSuccess);
}

void FcmTopicSubscriberImpl::ShutdownHandler() {
  NOTREACHED_IN_MIGRATION()
      << "FcmTopicSubscriberImpl should be destroyed before GCMDriver";
}

void FcmTopicSubscriberImpl::OnStoreReset() {}

void FcmTopicSubscriberImpl::OnMessage(const std::string& app_id,
                                       const gcm::IncomingMessage& message) {
  // Sender id is set to topic's name if message comes from subscribed topic
  if (!notification_callback_.is_null()) {
    notification_callback_.Run(message.sender_id.starts_with("/topics/"));
  }
}

void FcmTopicSubscriberImpl::OnMessagesDeleted(const std::string& app_id) {}

void FcmTopicSubscriberImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {}

void FcmTopicSubscriberImpl::OnSendAcknowledged(const std::string& app_id,
                                                const std::string& message_id) {
}

bool FcmTopicSubscriberImpl::CanHandle(const std::string& app_id) const {
  return app_id == app_id_;
}

}  // namespace ash::carrier_lock
