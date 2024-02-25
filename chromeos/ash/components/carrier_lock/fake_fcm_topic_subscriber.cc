// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/fake_fcm_topic_subscriber.h"

namespace ash::carrier_lock {

FakeFcmTopicSubscriber::FakeFcmTopicSubscriber() = default;

FakeFcmTopicSubscriber::~FakeFcmTopicSubscriber() = default;

bool FakeFcmTopicSubscriber::Initialize(NotificationCallback notification) {
  notification_callback_ = std::move(notification);
  return true;
}

void FakeFcmTopicSubscriber::RequestToken(Callback callback) {
  std::move(callback).Run(token_.empty() ? Result::kConnectionError
                                         : Result::kSuccess);
}

void FakeFcmTopicSubscriber::SubscribeTopic(const std::string& topic,
                                            Callback callback) {
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
