// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_FCM_TOPIC_SUBSCRIBER_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_FCM_TOPIC_SUBSCRIBER_H_

#include "chromeos/ash/components/carrier_lock/fcm_topic_subscriber.h"

namespace ash::carrier_lock {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    FakeFcmTopicSubscriber : public FcmTopicSubscriber {
 public:
  FakeFcmTopicSubscriber();
  ~FakeFcmTopicSubscriber() override;

  // FcmTopicSubscriber
  bool Initialize(NotificationCallback notification) override;
  void RequestToken(Callback callback) override;
  void SubscribeTopic(const std::string& topic,
                      Callback callback) override;
  std::string const token() override;

  void SetTokenAndResult(std::string token, Result result);
  void SendNotification();

 private:
  std::string token_;
  NotificationCallback notification_callback_;
  Result result_;
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_FCM_TOPIC_SUBSCRIBER_H_
