// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_IMPL_H_

#include "chromeos/components/phonehub/message_receiver.h"

#include "chromeos/components/phonehub/connection_manager.h"

namespace chromeos {
namespace phonehub {

// MessageReceiver implementation that observes all received messages from
// the remote phone and notifies clients of the received messages.
class MessageReceiverImpl : public MessageReceiver,
                            public ConnectionManager::Observer {
 public:
  MessageReceiverImpl(ConnectionManager* connection_manager);
  ~MessageReceiverImpl() override;

 private:
  // ConnectionManager::Observer:
  void OnMessageReceived(const std::string& payload) override;

  ConnectionManager* connection_manager_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_IMPL_H_
