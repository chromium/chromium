// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/text_message_provider.h"

#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

TextMessageProvider::TextMessageProvider() = default;

TextMessageProvider::~TextMessageProvider() = default;

void TextMessageProvider::Init(NetworkSmsHandler* network_sms_handler) {
  network_sms_handler_observer_.Observe(network_sms_handler);
}

void TextMessageProvider::MessageReceivedFromNetwork(
    const std::string& guid,
    const TextMessageData& message_data) {
  if (!ShouldAllowTextMessages(guid)) {
    NET_LOG(EVENT) << "Suppressing text message from network with guid: "
                   << guid;
    return;
  }

  NET_LOG(EVENT) << "Allowing text message from network with guid: " << guid;
  for (auto& observer : observers_) {
    observer.MessageReceived(message_data);
  }
}

bool TextMessageProvider::ShouldAllowTextMessages(const std::string& guid) {
  // TODO(b/290350602): Implement ShouldAllowTextMessages with policy.
  return true;
}

void TextMessageProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TextMessageProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
