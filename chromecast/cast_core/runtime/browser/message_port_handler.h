// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_HANDLER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_HANDLER_H_

#include <deque>
#include <memory>

#include "base/cancelable_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.grpc.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace chromecast {

class MessagePortService;

class MessagePortHandler final
    : public cast_api_bindings::MessagePort::Receiver {
 public:
  // |message_port_service|, |cq|, and |core_app_stub| should all outlive
  // |this|. Furthermore, |message_port_service| should actually own |this|.
  MessagePortHandler(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      uint32_t channel_id,
      MessagePortService* message_port_service,
      grpc::CompletionQueue* cq,
      cast::v2::CoreApplicationService::Stub* core_app_stub,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~MessagePortHandler() override;

  MessagePortHandler(const MessagePortHandler&) = delete;
  MessagePortHandler(MessagePortHandler&&) = delete;
  MessagePortHandler& operator=(const MessagePortHandler&) = delete;
  MessagePortHandler& operator=(MessagePortHandler&&) = delete;

  // Handles a message incoming from the gRPC API.  Returns true if it was able
  // to be handled successfully, false otherwise.
  bool HandleMessage(const cast::web::Message& message);

 private:
  class AsyncMessage final : public GrpcCall {
   public:
    AsyncMessage(const cast::web::Message& request,
                 cast::v2::CoreApplicationService::Stub* core_app_stub,
                 grpc::CompletionQueue* cq,
                 base::WeakPtr<MessagePortHandler> port);
    ~AsyncMessage() override;

    // GrpcCall overrides.
    void StepGRPC(grpc::Status status) override;

   private:
    base::WeakPtr<MessagePortHandler> port_;
    bool was_request_;

    cast::web::MessagePortStatus response_;
    std::unique_ptr<
        grpc::ClientAsyncResponseReader<cast::web::MessagePortStatus>>
        response_reader_;
  };

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
  bool ForwardMessage(cast::web::Message&& message);

  // Forwards |message| over this channel now.
  void ForwardMessageNow(const cast::web::Message& message);

  // Resets the timeout on the port that indicates we should close due to
  // inactivity.
  void ResetTimeout();

  // Callback invoked when an AsyncMessage gets a gRPC result.
  void OnMessageComplete(bool ok,
                         bool was_request,
                         const cast::web::MessagePortStatus& response);

  // cast_api_bindings::MessagePort::Receiver overrides.
  bool OnMessage(base::StringPiece message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  MessagePortService* message_port_service_;
  grpc::CompletionQueue* grpc_cq_;
  cast::v2::CoreApplicationService::Stub* core_app_stub_;
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
  uint32_t channel_id_;

  base::CancelableOnceClosure message_timeout_callback_;
  std::deque<cast::web::Message> pending_messages_;
  AsyncMessage* pending_request_{nullptr};
  bool awaiting_response_{false};
  bool started_{false};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MessagePortHandler> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_HANDLER_H_
