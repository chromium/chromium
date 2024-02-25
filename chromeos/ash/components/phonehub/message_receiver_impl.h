// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"

#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace phonehub {

// MessageReceiver implementation that observes all received messages from
// the remote phone and notifies clients of the received messages.
class MessageReceiverImpl : public MessageReceiver,
                            public secure_channel::ConnectionManager::Observer {
 public:
  explicit MessageReceiverImpl(
      secure_channel::ConnectionManager* connection_manager,
      PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger);
  ~MessageReceiverImpl() override;

 private:
  // secure_channel::ConnectionManager::Observer:
  void OnMessageReceived(const std::string& payload) override;

  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<PhoneHubStructuredMetricsLogger> phone_hub_structured_metrics_logger_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_IMPL_H_
