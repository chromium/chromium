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
      app_id_(app_id),
      sender_id_(sender_id),
      weakptr_factory_(this) {
  DCHECK(!app_id.empty());
  DCHECK(!sender_id.empty());
  if (gcm_driver) {
    if (!gcm_driver->GetAppHandler(app_id_)) {
      gcm_driver->AddAppHandler(app_id_, this);
    }
    instance_id_driver_ =
        std::make_unique<instance_id::InstanceIDDriver>(gcm_driver);
  }
}

FcmTopicSubscriberImpl::~FcmTopicSubscriberImpl() {
  if (gcm_driver_) {
    gcm_driver_->RemoveAppHandler(app_id_);
  }
}

void FcmTopicSubscriberImpl::SubscribeTopic(const std::string& topic,
                                            NotificationCallback notification,
                                            Callback callback) {
  if (request_callback_) {
    LOG(ERROR) << "FcmTopicSubscriberImpl cannot handle multiple requests.";
    std::move(callback).Run(Result::kHandlerBusy);
    return;
  }

  topic_ = topic;
  subscribe_callback_ = std::move(callback);

  if (token_.empty()) {
    RequestToken(std::move(notification),
                 base::BindOnce(&FcmTopicSubscriberImpl::Subscribe,
                                weakptr_factory_.GetWeakPtr()));
  } else {
    notification_callback_ = std::move(notification);
    Subscribe(Result::kSuccess);
  }
}

void FcmTopicSubscriberImpl::RequestToken(NotificationCallback notification,
                                          Callback callback) {
  if (request_callback_) {
    LOG(ERROR) << "FcmTopicSubscriberImpl cannot handle multiple requests.";
    std::move(callback).Run(Result::kHandlerBusy);
    return;
  }

  notification_callback_ = std::move(notification);
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
    ReturnSuccess();
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

void FcmTopicSubscriberImpl::Subscribe(Result request_token_result) {
  request_callback_ = std::move(subscribe_callback_);
  if (request_token_result != Result::kSuccess) {
    ReturnError(request_token_result);
    return;
  }
  if (topic_.empty()) {
    ReturnError(Result::kInvalidInput);
    return;
  }

  // TODO Implement SubscribeToTopic request in GCM driver
  ReturnSuccess();
}

void FcmTopicSubscriberImpl::OnSubscribe(
    instance_id::InstanceID::Result result) {
  if (result == instance_id::InstanceID::Result::SUCCESS) {
    ReturnSuccess();
    return;
  }

  LOG(ERROR) << "Failed to subscribe to FCM topic";
  switch (result) {
    case instance_id::InstanceID::INVALID_PARAMETER:
      ReturnError(Result::ERR_INVALID_INPUT);
      return;
    case instance_id::InstanceID::DISABLED:
      ReturnError(Result::ERR_INITIALIZATION);
      return;
    case instance_id::InstanceID::NETWORK_ERROR:
      ReturnError(Result::ERR_CONNECTION);
      return;
    case instance_id::InstanceID::SERVER_ERROR:
      ReturnError(Result::ERR_SERVER_INTERNAL);
      return;
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
    case instance_id::InstanceID::UNKNOWN_ERROR:
    default:
      ReturnError(Result::ERR_REQUEST_FAILED);
  }
}

void FcmTopicSubscriberImpl::ReturnError(Result err) {
  std::move(request_callback_).Run(err);
}

void FcmTopicSubscriberImpl::ReturnSuccess() {
  std::move(request_callback_).Run(Result::kSuccess);
}

void FcmTopicSubscriberImpl::ShutdownHandler() {
  if (gcm_driver_) {
    gcm_driver_->RemoveAppHandler(app_id_);
  }
  gcm_driver_ = nullptr;
}

void FcmTopicSubscriberImpl::OnStoreReset() {}

void FcmTopicSubscriberImpl::OnMessage(const std::string& app_id,
                                       const gcm::IncomingMessage& message) {
  // Sender id is set to topic's name if message comes from subscribed topic
  notification_callback_.Run(message.sender_id.starts_with("/topics/"));
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
