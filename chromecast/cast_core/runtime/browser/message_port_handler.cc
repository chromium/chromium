// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/message_port_handler.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "components/cast/message_port/platform_message_port.h"

namespace chromecast {
namespace {

// This is used as a timeout for both sending cast::web::Message requests and
// awaiting responses.  Reaching this timeout without a response from the peer
// will close the connection and Blink message port.
constexpr base::TimeDelta kMessageTimeout = base::Seconds(10);

}  // namespace

#define DLOG_CHANNEL(level) DLOG(level) << "channel " << channel_id_ << ": "
#define DVLOG_CHANNEL(level) DVLOG(level) << "channel " << channel_id_ << ": "

MessagePortHandler::MessagePortHandler(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    uint32_t channel_id,
    MessagePortService* message_port_service,
    grpc::CompletionQueue* cq,
    cast::v2::CoreApplicationService::Stub* core_app_stub,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      message_port_service_(message_port_service),
      grpc_cq_(cq),
      core_app_stub_(core_app_stub),
      message_port_(std::move(message_port)),
      channel_id_(channel_id) {
  DCHECK(message_port_service_);
  DCHECK(grpc_cq_);
  DCHECK(core_app_stub_);
  message_port_->SetReceiver(this);
}

MessagePortHandler::~MessagePortHandler() = default;

bool MessagePortHandler::HandleMessage(const cast::web::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!message_port_) {
    return false;
  }

  switch (message.message_type_case()) {
    case cast::web::Message::kStatus: {
      if (message.status().status() ==
          cast::web::MessagePortStatus_Status_ERROR) {
        DLOG_CHANNEL(WARNING) << "Received error message";
        CloseAndRemove();
      } else if (message.status().status() ==
                 cast::web::MessagePortStatus_Status_STARTED) {
        bool was_started = started_;
        started_ = true;
        if (!was_started && !pending_messages_.empty()) {
          ForwardNextMessage();
        }
      }
      return true;
    }
    case cast::web::Message::kRequest: {
      DLOG_CHANNEL(INFO) << "Received request: " << message.request().data();
      std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports;
      ports.reserve(message.request().ports_size());
      for (const auto& port : message.request().ports()) {
        std::unique_ptr<cast_api_bindings::MessagePort> client;
        std::unique_ptr<cast_api_bindings::MessagePort> server;
        cast_api_bindings::CreatePlatformMessagePortPair(&client, &server);
        message_port_service_->RegisterIncomingPort(port.channel().channel_id(),
                                                    std::move(client));
        ports.push_back(std::move(server));

        cast::web::Message notification;
        notification.mutable_channel()->set_channel_id(
            port.channel().channel_id());
        notification.mutable_status()->set_status(
            cast::web::MessagePortStatus_Status_STARTED);
        ForwardMessage(std::move(notification));
      }
      bool result = message_port_->PostMessageWithTransferables(
          message.request().data(), std::move(ports));
      SendResponse(result);
      return true;
    }
    case cast::web::Message::kResponse: {
      if (!awaiting_response_) {
        LOG(FATAL) << "Received response while not expecting one.";
        return false;
      }
      message_timeout_callback_.Cancel();
      awaiting_response_ = false;
      if (!pending_messages_.empty() && !pending_request_) {
        ForwardNextMessage();
      }
      return true;
    }
    default:
      return false;
  }
}

void MessagePortHandler::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG_CHANNEL(INFO) << "Closing channel";
  message_timeout_callback_.Cancel();
  pending_messages_.clear();
  message_port_->Close();
  message_port_.reset();
}

void MessagePortHandler::CloseAndRemove() {
  Close();
  message_port_service_->Remove(channel_id_);
}

void MessagePortHandler::CloseWithError(CloseError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (error) {
    case CloseError::kPipeError:
      DLOG_CHANNEL(INFO) << "Closing with pipe error";
      break;
    case CloseError::kTimeout:
      DLOG_CHANNEL(INFO) << "Closing from timeout";
      break;
  }
  Close();

  cast::web::Message message;
  message.mutable_status()->set_status(
      cast::web::MessagePortStatus_Status_ERROR);
  message.mutable_channel()->set_channel_id(channel_id_);
  new AsyncMessage(message, core_app_stub_, grpc_cq_, nullptr);
  message_port_service_->Remove(channel_id_);
}

void MessagePortHandler::SendResponse(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cast::web::Message message;
  message.mutable_response()->set_result(result);
  message.mutable_channel()->set_channel_id(channel_id_);
  ForwardMessage(std::move(message));
}

void MessagePortHandler::ForwardNextMessage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!awaiting_response_);
  DCHECK(!pending_messages_.empty());
  DCHECK(!pending_request_);
  cast::web::Message next = std::move(pending_messages_.front());
  pending_messages_.pop_front();
  ForwardMessageNow(std::move(next));
}

bool MessagePortHandler::ForwardMessage(cast::web::Message&& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (message.has_request() &&
      (!started_ || awaiting_response_ || !pending_messages_.empty() ||
       pending_request_)) {
    pending_messages_.emplace_back(std::move(message));
    return true;
  }

  ForwardMessageNow(message);
  return true;
}

void MessagePortHandler::ForwardMessageNow(const cast::web::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!message.has_request() || !pending_request_);
  auto* async_message = new AsyncMessage(message, core_app_stub_, grpc_cq_,
                                         weak_factory_.GetWeakPtr());
  if (message.has_request()) {
    DLOG_CHANNEL(INFO) << "Sending message: " << message.request().data();
    pending_request_ = async_message;
    awaiting_response_ = true;
  }
  ResetTimeout();
}

void MessagePortHandler::ResetTimeout() {
  message_timeout_callback_.Reset(
      base::BindOnce(&MessagePortHandler::CloseWithError,
                     weak_factory_.GetWeakPtr(), CloseError::kTimeout));
  task_runner_->PostDelayedTask(FROM_HERE, message_timeout_callback_.callback(),
                                kMessageTimeout);
}

void MessagePortHandler::OnMessageComplete(
    bool ok,
    bool was_request,
    const cast::web::MessagePortStatus& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_request_ = nullptr;
  message_timeout_callback_.Cancel();
  if (!message_port_) {
    return;
  }

  if (!ok || response.status() != cast::web::MessagePortStatus_Status_OK) {
    DLOG_CHANNEL(WARNING) << "Send failed (" << ok << ", "
                          << cast::web::MessagePortStatus_Status_Name(
                                 response.status())
                          << ")";
    CloseAndRemove();
    return;
  }

  if (was_request && awaiting_response_) {
    ResetTimeout();
  } else if (!awaiting_response_ && !pending_messages_.empty()) {
    ForwardNextMessage();
  }
}

bool MessagePortHandler::OnMessage(
    base::StringPiece message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cast::web::Message request;
  request.mutable_request()->set_data(std::string(message));
  request.mutable_channel()->set_channel_id(channel_id_);
  std::vector<cast::web::Message> started_notifications;
  started_notifications.reserve(ports.size());
  for (auto& port : ports) {
    auto* descriptor = request.mutable_request()->mutable_ports()->Add();
    uint32_t channel_id =
        message_port_service_->RegisterOutgoingPort(std::move(port));
    descriptor->mutable_channel()->set_channel_id(channel_id);
    descriptor->set_sequence_number(0);

    cast::web::Message notification;
    notification.mutable_channel()->set_channel_id(channel_id);
    notification.mutable_status()->set_status(
        cast::web::MessagePortStatus_Status_STARTED);
    started_notifications.push_back(std::move(notification));
  }
  ForwardMessage(std::move(request));
  for (auto& notification : started_notifications) {
    ForwardMessage(std::move(notification));
  }
  return true;
}

void MessagePortHandler::OnPipeError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseWithError(CloseError::kPipeError);
}

MessagePortHandler::AsyncMessage::AsyncMessage(
    const cast::web::Message& request,
    cast::v2::CoreApplicationService::Stub* core_app_stub,
    grpc::CompletionQueue* cq,
    base::WeakPtr<MessagePortHandler> port)
    : port_(port), was_request_(request.has_request()) {
  response_reader_ =
      core_app_stub->PrepareAsyncPostMessage(&context_, request, cq);
  response_reader_->StartCall();
  response_reader_->Finish(&response_, &status_, static_cast<GRPC*>(this));
}

MessagePortHandler::AsyncMessage::~AsyncMessage() = default;

void MessagePortHandler::AsyncMessage::StepGRPC(grpc::Status status) {
  if (port_) {
    port_->OnMessageComplete(status_.ok() && status.ok(), was_request_,
                             response_);
  }
  delete this;
}

}  // namespace chromecast
