// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/message_port_handler.h"

#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/public/message_port_service.h"

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
    cast_receiver::MessagePortService* message_port_service,
    cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      message_port_service_(message_port_service),
      core_app_stub_(core_app_stub),
      message_port_(std::move(message_port)),
      channel_id_(channel_id) {
  DCHECK(message_port_service_);
  DCHECK(core_app_stub_);
  message_port_->SetReceiver(this);
}

MessagePortHandler::~MessagePortHandler() = default;

cast_receiver::Status MessagePortHandler::HandleMessage(
    const cast::web::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!message_port_) {
    return cast_receiver::Status(cast_receiver::StatusCode::kFailedPrecondition,
                                 "Invalid MessagePort");
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
      return cast_receiver::OkStatus();
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
      return cast_receiver::OkStatus();
    }
    case cast::web::Message::kResponse: {
      if (!is_awaiting_response_) {
        LOG(FATAL) << "Received response while not expecting one.";
      }
      message_timeout_callback_.Cancel();
      is_awaiting_response_ = false;
      if (!pending_messages_.empty() && !has_outstanding_request_) {
        ForwardNextMessage();
      }
      return cast_receiver::OkStatus();
    }
    default:
      return cast_receiver::Status(cast_receiver::StatusCode::kInvalidArgument,
                                   "Invalid cast::web::Message");
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

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreMessagePortApplicationServiceStub::PostMessage>();
  call.request().mutable_status()->set_status(
      cast::web::MessagePortStatus_Status_ERROR);
  call.request().mutable_channel()->set_channel_id(channel_id_);
  std::move(call).InvokeAsync(base::DoNothing());

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
  DCHECK(!is_awaiting_response_);
  DCHECK(!pending_messages_.empty());
  DCHECK(!has_outstanding_request_);
  cast::web::Message next = std::move(pending_messages_.front());
  pending_messages_.pop_front();
  ForwardMessageNow(std::move(next));
}

bool MessagePortHandler::ForwardMessage(cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (message.has_request() &&
      (!started_ || is_awaiting_response_ || !pending_messages_.empty() ||
       has_outstanding_request_)) {
    pending_messages_.emplace_back(std::move(message));
    return true;
  }

  ForwardMessageNow(std::move(message));
  return true;
}

void MessagePortHandler::ForwardMessageNow(cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!message.has_request() || !has_outstanding_request_);
  bool was_request = message.has_request();

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreMessagePortApplicationServiceStub::PostMessage>(
      std::move(message));
  std::move(call).InvokeAsync(base::BindPostTask(
      task_runner_, base::BindOnce(&MessagePortHandler::OnPortMessagePosted,
                                   weak_factory_.GetWeakPtr(), was_request)));
  if (was_request) {
    has_outstanding_request_ = true;
    is_awaiting_response_ = true;
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

void MessagePortHandler::OnPortMessagePosted(
    bool was_request,
    cast::utils::GrpcStatusOr<cast::web::MessagePortStatus> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_outstanding_request_ = false;
  message_timeout_callback_.Cancel();
  if (!message_port_) {
    return;
  }

  if (!response_or.ok() ||
      response_or->status() != cast::web::MessagePortStatus_Status_OK) {
    DLOG_CHANNEL(WARNING) << "Send failed (" << response_or.ToString() << ", "
                          << cast::web::MessagePortStatus_Status_Name(
                                 response_or->status())
                          << ")";
    CloseAndRemove();
    return;
  }

  if (was_request && is_awaiting_response_) {
    ResetTimeout();
  } else if (!is_awaiting_response_ && !pending_messages_.empty()) {
    ForwardNextMessage();
  }
}

bool MessagePortHandler::OnMessage(
    std::string_view message,
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

}  // namespace chromecast
