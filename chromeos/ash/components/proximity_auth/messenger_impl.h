// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_IMPL_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/ash/components/proximity_auth/messenger.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"

namespace proximity_auth {

// Concrete implementation of the Messenger interface.
class MessengerImpl : public Messenger,
                      public ash::secure_channel::ClientChannel::Observer {
 public:
  // Constructs a messenger that sends and receives messages.
  //
  // Messages are relayed over the provided |channel|.
  //
  // The messenger begins observing messages as soon as it is constructed.
  explicit MessengerImpl(
      std::unique_ptr<ash::secure_channel::ClientChannel> channel);

  MessengerImpl(const MessengerImpl&) = delete;
  MessengerImpl& operator=(const MessengerImpl&) = delete;

  ~MessengerImpl() override;

  // Messenger:
  void AddObserver(MessengerObserver* observer) override;
  void RemoveObserver(MessengerObserver* observer) override;
  void DispatchUnlockEvent() override;
  void RequestUnlock() override;
  ash::secure_channel::ClientChannel* GetChannel() const override;

 private:
  // Internal data structure to represent a pending message that either hasn't
  // been sent yet or is waiting for a response from the remote device.
  struct PendingMessage {
    PendingMessage();
    explicit PendingMessage(const base::Value::Dict& message);
    explicit PendingMessage(const std::string& message);
    ~PendingMessage();

    // The message, serialized as JSON.
    const std::string json_message;

    // The message type. This is possible to parse from the |json_message|; it's
    // stored redundantly for convenience.
    const std::string type;
  };

  // Pops the first of the |queued_messages_| and sends it to the remote device.
  void ProcessMessageQueue();

  // Called when the message is encoded so it can be sent over the connection.
  void OnMessageEncoded(const std::string& encoded_message);

  // Handles an incoming "status_update" |message|, parsing and notifying
  // observers of the content.
  void HandleRemoteStatusUpdateMessage(const base::Value::Dict& message);

  // Handles an incoming "unlock_response" message, notifying observers of the
  // response.
  void HandleUnlockResponseMessage(const base::Value::Dict& message);

  // ash::secure_channel::ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  // Called when a message has been recevied from the remote device. The message
  // should be a valid JSON string.
  void HandleMessage(const std::string& message);

  // Called when a message has been sent to the remote device.
  void OnSendMessageResult(bool success);

  // Authenticated end-to-end channel used to communicate with the remote
  // device.
  std::unique_ptr<ash::secure_channel::ClientChannel> channel_;

  // The registered observers of |this_| messenger.
  base::ObserverList<MessengerObserver>::Unchecked observers_;

  // Queue of messages to send to the remote device.
  base::circular_deque<PendingMessage> queued_messages_;

  // The current message being sent or waiting on the remote device for a
  // response. Null if there is no message currently in this state.
  std::unique_ptr<PendingMessage> pending_message_;

  base::WeakPtrFactory<MessengerImpl> weak_ptr_factory_{this};
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_IMPL_H_
