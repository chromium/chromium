// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_HANDLER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_HANDLER_H_

#include <deque>
#include <memory>
#include <string_view>

#include "base/cancelable_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace cast_receiver {
class MessagePortService;
}

namespace chromecast {

class MessagePortHandler final
    : public cast_api_bindings::MessagePort::Receiver {
 public:
  // |message_port_service|, |cq|, and |core_app_stub| should all outlive
  // |this|. Furthermore, |message_port_service| should actually own |this|.
  MessagePortHandler(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      uint32_t channel_id,
      cast_receiver::MessagePortService* message_port_service,
      cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~MessagePortHandler() override;

  MessagePortHandler(const MessagePortHandler&) = delete;
  MessagePortHandler(MessagePortHandler&&) = delete;
  MessagePortHandler& operator=(const MessagePortHandler&) = delete;
  MessagePortHandler& operator=(MessagePortHandler&&) = delete;

  // Handles a message incoming from the gRPC API.  Returns true if it was able
  // to be handled successfully, false otherwise.
  cast_receiver::Status HandleMessage(const cast::web::Message& message);

 private:
  enum class CloseError {
    kPipeError,
    kTimeout,
  };

  // Closes the message port.
  void Close();

  // Closes the message port and removes this channel from
  // |message_port_service_|.
  void CloseAndRemove();

  // Closes the message port, sends an error on the channel, and removes this
  // channel from |message_port_service_|.
  void CloseWithError(CloseError error);

  // Sends a MessageResponse with the value |result|.
  void SendResponse(bool result);

  // Forwards the next message from |pending_messages_| over this channel.
  void ForwardNextMessage();

  // Forwards |message| over this channel if possible, but queues it if there's
  // already a pending request.
  bool ForwardMessage(cast::web::Message message);

  // Forwards |message| over this channel now.
  void ForwardMessageNow(cast::web::Message message);

  // Resets the timeout on the port that indicates we should close due to
  // inactivity.
  void ResetTimeout();

  // Callback invoked when an AsyncMessage gets a gRPC result.
  void OnPortMessagePosted(
      bool was_request,
      cast::utils::GrpcStatusOr<cast::web::MessagePortStatus> response_or);

  // cast_api_bindings::MessagePort::Receiver overrides.
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  cast_receiver::MessagePortService* const message_port_service_;
  cast::v2::CoreMessagePortApplicationServiceStub* const core_app_stub_;
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
  uint32_t channel_id_;

  base::CancelableOnceClosure message_timeout_callback_;
  std::deque<cast::web::Message> pending_messages_;
  bool has_outstanding_request_{false};
  bool is_awaiting_response_{false};
  bool started_{false};

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MessagePortHandler> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_HANDLER_H_
