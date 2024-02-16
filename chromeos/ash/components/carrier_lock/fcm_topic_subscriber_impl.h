// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FCM_TOPIC_SUBSCRIBER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FCM_TOPIC_SUBSCRIBER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/carrier_lock/fcm_topic_subscriber.h"
#include "chromeos/ash/components/carrier_lock/topic_subscription_request.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace ash::carrier_lock {

// This class handles the registration to Firebase Cloud Messaging service and
// subscription to specified public topic for receiving asynchronous
// notifications when the Carrier Lock configuration changes.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    FcmTopicSubscriberImpl : public FcmTopicSubscriber,
                             public gcm::GCMAppHandler {
 public:
  FcmTopicSubscriberImpl(gcm::GCMDriver* gcm_driver,
                         std::string app_id,
                         std::string sender_id,
                         scoped_refptr<network::SharedURLLoaderFactory>);
  ~FcmTopicSubscriberImpl() override;
  FcmTopicSubscriberImpl() = delete;

  // FcmTopicSubscriber
  bool Initialize(NotificationCallback notification) override;
  void RequestToken(Callback callback) override;
  void SubscribeTopic(const std::string& topic,
                      Callback callback) override;
  std::string const token() override;

 private:
  friend class FcmTopicSubscriberTest;

  void Subscribe(Result request_token_result);
  void OnGetToken(const std::string& token,
                  instance_id::InstanceID::Result result);
  void OnGetGcmStatistics(const gcm::GCMClient::GCMStatistics& statistics);

  void ReturnError(Result);
  void ReturnSuccess();

  // GCMAppHandler
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  bool CanHandle(const std::string& app_id) const override;

  raw_ptr<gcm::GCMDriver> gcm_driver_;
  std::unique_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  std::unique_ptr<TopicSubscriptionRequest> subscription_request_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  uint64_t android_id_ = 0;
  uint64_t android_secret_ = 0;
  std::string app_id_;
  std::string sender_id_;
  std::string token_;
  std::string topic_;
  Callback request_callback_;
  Callback subscribe_callback_;
  NotificationCallback notification_callback_;

  base::WeakPtrFactory<FcmTopicSubscriberImpl> weakptr_factory_{this};
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FCM_TOPIC_SUBSCRIBER_IMPL_H_
