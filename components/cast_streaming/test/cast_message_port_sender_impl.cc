// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/test/cast_message_port_sender_impl.h"

#include "base/logging.h"
#include "components/cast_streaming/common/message_serialization.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace cast_streaming {

CastMessagePortSenderImpl::CastMessagePortSenderImpl(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    base::OnceClosure on_close,
    base::OnceClosure on_system_sender_message_received)
    : message_port_(std::move(message_port)),
      on_close_(std::move(on_close)),
      on_system_sender_message_received_(
          std::move(on_system_sender_message_received)) {
  VLOG(1) << __func__;
  message_port_->SetReceiver(this);
}

CastMessagePortSenderImpl::~CastMessagePortSenderImpl() = default;

void CastMessagePortSenderImpl::MaybeClose() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We may be called multiple times, but only want to close once.
  if (is_closed_) {
    return;
  }

  if (message_port_) {
    message_port_.reset();
  }
  if (client_) {
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2CastSocketError));
    if (on_close_) {
      std::move(on_close_).Run();
    }
  }
  is_closed_ = true;
}

void CastMessagePortSenderImpl::SetClient(
    openscreen::cast::MessagePort::Client& client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  client_ = &client;
}

void CastMessagePortSenderImpl::ResetClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_ = nullptr;
  MaybeClose();
}

void CastMessagePortSenderImpl::PostMessage(
    const std::string& sender_id,
    const std::string& message_namespace,
    const std::string& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(3) << __func__;
  if (!message_port_) {
    return;
  }

  VLOG(3) << "Received Open Screen message. SenderId: " << sender_id
          << ". Namespace: " << message_namespace << ". Message: " << message;
  message_port_->PostMessage(
      SerializeCastMessage(sender_id, message_namespace, message));
}

bool CastMessagePortSenderImpl::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  VLOG(3) << __func__;

  // If |client_| was cleared, |message_port_| should have been reset.
  CHECK(client_);

  if (!ports.empty()) {
    // We should never receive any ports for Cast Streaming.
    MaybeClose();
    return false;
  }

  std::string sender_id;
  std::string message_namespace;
  std::string str_message;
  if (!DeserializeCastMessage(message, &sender_id, &message_namespace,
                              &str_message)) {
    LOG(ERROR) << "Received bad message. " << message;
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2InvalidMessage));
    return false;
  }
  VLOG(3) << "Received Cast message. SenderId: " << sender_id
          << ". Namespace: " << message_namespace
          << ". Message: " << str_message;

  if (message_namespace == kSystemNamespace) {
    if (on_system_sender_message_received_) {
      std::move(on_system_sender_message_received_).Run();
    }
  } else {
    client_->OnMessage(sender_id, message_namespace, str_message);
  }

  return true;
}

void CastMessagePortSenderImpl::OnPipeError() {
  VLOG(3) << __func__;
  MaybeClose();
}

}  // namespace cast_streaming
