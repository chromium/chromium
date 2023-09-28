// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/fake_fcm_topic_subscriber.h"

namespace ash::carrier_lock {

FakeFcmTopicSubscriber::FakeFcmTopicSubscriber() = default;

FakeFcmTopicSubscriber::~FakeFcmTopicSubscriber() = default;

void FakeFcmTopicSubscriber::RequestToken(NotificationCallback notification,
                                          Callback callback) {
  notification_callback_ = std::move(notification);
  std::move(callback).Run(token_.empty() ? Result::kConnectionError
                                         : Result::kSuccess);
}

void FakeFcmTopicSubscriber::SubscribeTopic(const std::string& topic,
                                            NotificationCallback notification,
                                            Callback callback) {
  notification_callback_ = std::move(notification);
  std::move(callback).Run(result_);
}

void FakeFcmTopicSubscriber::SetTokenAndResult(std::string token,
                                               Result result) {
  token_ = token;
  result_ = result;
}

void FakeFcmTopicSubscriber::SendNotification() {
  notification_callback_.Run(false);
}

std::string const FakeFcmTopicSubscriber::token() {
  return token_;
}

}  // namespace ash::carrier_lock
