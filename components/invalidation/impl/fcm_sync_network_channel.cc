// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_sync_network_channel.h"

#include "base/logging.h"
#include "base/observer_list.h"

namespace invalidation {

FCMSyncNetworkChannel::FCMSyncNetworkChannel() : received_messages_count_(0) {}

FCMSyncNetworkChannel::~FCMSyncNetworkChannel() = default;

void FCMSyncNetworkChannel::SetMessageReceiver(
    MessageCallback incoming_receiver) {
  incoming_receiver_ = std::move(incoming_receiver);
}

void FCMSyncNetworkChannel::SetTokenReceiver(TokenCallback token_receiver) {
  token_receiver_ = token_receiver;
  if (!token_.empty())
    token_receiver_.Run(token_);
}

void FCMSyncNetworkChannel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FCMSyncNetworkChannel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FCMSyncNetworkChannel::NotifyChannelStateChange(FcmChannelState state) {
  for (auto& observer : observers_)
    observer.OnFCMChannelStateChanged(state);
}

bool FCMSyncNetworkChannel::DeliverIncomingMessage(
    const std::string& payload,
    const std::string& private_topic,
    const std::string& public_topic,
    int64_t version) {
  if (!incoming_receiver_) {
    DLOG(ERROR) << "No receiver for incoming notification";
    return false;
  }
  received_messages_count_++;
  incoming_receiver_.Run(payload, private_topic, public_topic, version);
  return true;
}

bool FCMSyncNetworkChannel::DeliverToken(const std::string& token) {
  token_ = token;
  if (!token_receiver_) {
    DLOG(ERROR) << "No receiver for token";
    return false;
  }
  token_receiver_.Run(token_);
  return true;
}

}  // namespace invalidation
