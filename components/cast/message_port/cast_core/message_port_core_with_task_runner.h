// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_PORT_CORE_WITH_TASK_RUNNER_H_
#define COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_PORT_CORE_WITH_TASK_RUNNER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast/message_port/cast_core/message_port_core.h"

namespace cast_api_bindings {

// MessagePortCore serving users of
// base::SequencedTaskRunner::CurrentDefaultHandle
class MessagePortCoreWithTaskRunner : public MessagePortCore {
 public:
  explicit MessagePortCoreWithTaskRunner(uint32_t channel_id);
  MessagePortCoreWithTaskRunner(const MessagePortCoreWithTaskRunner&) = delete;
  MessagePortCoreWithTaskRunner(MessagePortCoreWithTaskRunner&& other);
  ~MessagePortCoreWithTaskRunner() override;
  MessagePortCoreWithTaskRunner& operator=(
      const MessagePortCoreWithTaskRunner&) = delete;
  MessagePortCoreWithTaskRunner& operator=(
      MessagePortCoreWithTaskRunner&& other);

  // Creates a pair of bound MessagePorts representing two ends of a channel.
  static std::pair<MessagePortCoreWithTaskRunner, MessagePortCoreWithTaskRunner>
  CreatePair();

 private:
  // MessagePortCore implementation:
  void AcceptOnSequence(Message message) override;
  void AcceptResultOnSequence(bool result) override;
  void CheckPeerStartedOnSequence() override;
  void StartOnSequence() override;
  void PostMessageOnSequence(Message message) override;
  void OnPipeErrorOnSequence() override;
  void SetTaskRunner() override;
  bool HasTaskRunner() const override;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_);

  base::WeakPtrFactory<MessagePortCoreWithTaskRunner> weak_factory_{this};
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_PORT_CORE_WITH_TASK_RUNNER_H_
