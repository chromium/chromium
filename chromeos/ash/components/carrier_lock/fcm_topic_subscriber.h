// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FCM_TOPIC_SUBSCRIBER_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FCM_TOPIC_SUBSCRIBER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/carrier_lock/common.h"

namespace ash::carrier_lock {

// This class handles the registration to Firebase Cloud Messaging service and
// subscription to specified public topic for receiving asynchronous
// notifications when the Carrier Lock configuration changes.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    FcmTopicSubscriber {
 public:
  using NotificationCallback = base::RepeatingCallback<void(bool from_topic)>;

  FcmTopicSubscriber() = default;
  virtual ~FcmTopicSubscriber() = default;

  // Initialize notification handler
  virtual bool Initialize(NotificationCallback notification) = 0;

  // Register to FCM and request a Token
  virtual void RequestToken(Callback callback) = 0;

  // Verify Token and subscribe to notification topic
  virtual void SubscribeTopic(const std::string& topic,
                              Callback callback) = 0;

  // Return the Token received in response
  virtual std::string const token() = 0;

 protected:
  friend class FcmTopicSubscriberTest;

  void set_is_testing(bool testing) { is_testing_ = testing; }
  bool is_testing() { return is_testing_; }
  bool is_testing_ = false;
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FCM_TOPIC_SUBSCRIBER_H_
