// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_PORT_CORE_H_
#define COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_PORT_CORE_H_

#include <memory>
#include <queue>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/cast/message_port/cast_core/message_connector.h"
#include "components/cast/message_port/message_port.h"

namespace cast_api_bindings {

// A serialized version of the port used for transfer.
struct MessagePortDescriptor {
  MessagePortDescriptor(uint32_t channel_id, bool peer_started);
  MessagePortDescriptor(MessagePortDescriptor&& other);
  uint32_t channel_id;
  bool peer_started;
};

class MessagePortCore;

// Message sent over a MessagePortCore, containing data and optional
// MessagePortCore(s).
struct Message {
 public:
  Message(const Message&) = delete;
  Message(Message&&);
  Message& operator=(const Message&) = delete;
  Message& operator=(Message&&);
  explicit Message(const std::string& data);
  explicit Message(const std::string& data,
                   std::vector<std::unique_ptr<MessagePortCore>> port);
  ~Message();

  std::string data;
  std::vector<std::unique_ptr<MessagePortCore>> ports;
};

// MessagePortCore used for transferring Message between a
// Receiver and another MessageConnector. Overrides of MessagePort and
// MessageConnector functions are run on a sequence provided by the OnSequence
// methods.
class MessagePortCore : public MessagePort, public MessageConnector {
 public:
  explicit MessagePortCore(uint32_t channel_id);
  MessagePortCore(const MessagePortCore&) = delete;
  MessagePortCore(MessagePortCore&& other);
  ~MessagePortCore() override;

  // Gets the MessagePortCore representation of |port|
  // for callers who are certain of its type. Typically used when |port|
  // is being transferred.
  static MessagePortCore* FromMessagePort(MessagePort* port);

  // MessagePortCore does not have a service to manage transfer of its handles;
  // the connector must be replaced, e.g. with a remote connector.
  MessagePortDescriptor Transfer(MessageConnector* replacement);

  // MessagePort implementation:
  bool PostMessage(std::string_view message) override;
  bool PostMessageWithTransferables(
      std::string_view message,
      std::vector<std::unique_ptr<MessagePort>> ports) override;
  void SetReceiver(MessagePort::Receiver* receiver) override;
  void Close() override;
  bool CanPostMessage() const override;

 protected:
  void Assign(MessagePortCore&& other);

  // Whether the port is valid and connected to a peer.
  bool IsValid() const;

  // Implementation must be able to schedule Accept, AcceptResult, PostMessage,
  // and OnPipeError on a sequence.
  virtual void AcceptOnSequence(Message message) = 0;
  virtual void AcceptResultOnSequence(bool result) = 0;
  virtual void CheckPeerStartedOnSequence() = 0;
  virtual void StartOnSequence() = 0;
  virtual void PostMessageOnSequence(Message message) = 0;
  virtual void OnPipeErrorOnSequence() = 0;
  virtual void SetTaskRunner() = 0;
  virtual bool HasTaskRunner() const = 0;

  // Synchronous operations used by Accept, AcceptResult, PostMessage, and
  // OnPipeError to operate on a sequence.
  void AcceptInternal(Message message);
  void AcceptResultInternal(bool result);
  void CheckPeerStartedInternal();
  void PostMessageInternal(Message message);
  void OnPipeErrorInternal();

 private:
  // Posts the next message on the queue, if there is one. Should only be
  // invoked on the sequence.
  void ProcessMessageQueue();

  // MessageConnector implementation:
  bool Accept(Message message) override;
  bool AcceptResult(bool result) override;
  void OnPeerStarted() override;
  void OnPeerError() override;

  raw_ptr<MessagePort::Receiver> receiver_ = nullptr;
  bool pending_response_ = false;
  bool errored_ = false;
  bool closed_ = false;
  bool peer_started_ = false;
  std::queue<Message> message_queue_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_PORT_CORE_H_
