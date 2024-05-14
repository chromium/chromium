// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"

#include <lib/fpromise/result.h>

#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"

namespace cast_api_bindings {
namespace {

// A MessagePortFuchsia instantiated with an
// InterfaceHandle<fuchsia::web::MessagePort>. Acts as a client of a
// MessagePortFuchsiaServer.
class MessagePortFuchsiaClient : public MessagePortFuchsia {
 public:
  explicit MessagePortFuchsiaClient(
      fidl::InterfaceHandle<::fuchsia::web::MessagePort> port)
      : MessagePortFuchsia(PortType::HANDLE), port_(port.Bind()) {}

  ~MessagePortFuchsiaClient() override {}

  // MessagePortFuchsia implementation.
  fidl::InterfaceHandle<::fuchsia::web::MessagePort> TakeClientHandle() final {
    CHECK(!receiver_);
    CHECK(port_.is_bound());
    return port_.Unbind();
  }

  fidl::InterfaceRequest<::fuchsia::web::MessagePort> TakeServiceRequest()
      final {
    NOTREACHED_IN_MIGRATION();
    return {};
  }

  // MessagePort implementation.
  void SetReceiver(cast_api_bindings::MessagePort::Receiver* receiver) final {
    CHECK(receiver);
    CHECK(!receiver_);
    receiver_ = receiver;
    port_.set_error_handler(
        [this](zx_status_t status) { MessagePortFuchsia::OnZxError(status); });
    ReadNextMessage();
  }

  void Close() final {
    if (port_.is_bound()) {
      port_.Unbind();
    }
  }

  bool CanPostMessage() const final { return port_.is_bound(); }

 private:
  void OnDeliverMessageToFidlComplete(
      fuchsia::web::MessagePort_PostMessage_Result result) {
    if (result.is_err()) {
      LOG(ERROR) << "PostMessage failed, reason: "
                 << static_cast<int32_t>(result.err());
      ReportPipeError();
      return;
    }

    message_queue_.pop_front();
    DeliverMessageToFidl();
  }

  void DeliverMessageToFidl() final {
    if (message_queue_.empty()) {
      return;
    }

    port_->PostMessage(
        std::move(message_queue_.front()),
        fit::bind_member(
            this, &MessagePortFuchsiaClient::OnDeliverMessageToFidlComplete));
  }

  // Helpers for reading and writing messages on the |port_|
  void OnMessageReady(fuchsia::web::WebMessage message) {
    base::WeakPtr<MessagePortFuchsia> weak_this =
        weak_factory_.GetMutableWeakPtr();
    auto status = ExtractAndHandleMessageFromFidl(std::move(message));
    if (!weak_this)
      return;

    if (status) {
      LOG(WARNING) << "Received bad message, error: "
                   << static_cast<int32_t>(*status);
      ReportPipeError();
      return;
    }

    ReadNextMessage();
  }

  void ReadNextMessage() {
    CHECK(receiver_);
    CHECK(port_);

    port_->ReceiveMessage(
        fit::bind_member(this, &MessagePortFuchsiaClient::OnMessageReady));
  }

  fuchsia::web::MessagePortPtr port_;

  const base::WeakPtrFactory<MessagePortFuchsia> weak_factory_{this};
};

// A MessagePortFuchsia instantiated with an
// InterfaceRequest<fuchsia::web::MessagePort>. Acts as the server for a
// MessagePortFuchsiaClient.
class MessagePortFuchsiaServer : public MessagePortFuchsia,
                                 public fuchsia::web::MessagePort {
 public:
  explicit MessagePortFuchsiaServer(
      fidl::InterfaceRequest<::fuchsia::web::MessagePort> port)
      : MessagePortFuchsia(PortType::REQUEST), binding_(this) {
    binding_.Bind(std::move(port));
  }

  ~MessagePortFuchsiaServer() override {}

  // MessagePortFuchsia implementation.
  fidl::InterfaceHandle<::fuchsia::web::MessagePort> TakeClientHandle() final {
    NOTREACHED_IN_MIGRATION();
    return {};
  }

  fidl::InterfaceRequest<::fuchsia::web::MessagePort> TakeServiceRequest()
      final {
    return binding_.Unbind();
  }

  // MessagePort implementation.
  void SetReceiver(cast_api_bindings::MessagePort::Receiver* receiver) final {
    CHECK(receiver);
    CHECK(!receiver_);
    receiver_ = receiver;
    binding_.set_error_handler(
        [this](zx_status_t status) { MessagePortFuchsia::OnZxError(status); });
  }

  void Close() final {
    if (binding_.is_bound()) {
      binding_.Unbind();
    }
  }

  bool CanPostMessage() const final { return binding_.is_bound(); }

 private:
  void DeliverMessageToFidl() final {
    // Do nothing if the client hasn't requested a read, or if there's nothing
    // to read.
    if (!pending_receive_message_callback_)
      return;

    if (message_queue_.empty())
      return;

    pending_receive_message_callback_(std::move(message_queue_.front()));
    pending_receive_message_callback_ = {};
    message_queue_.pop_front();
  }

  // Avoid hiding warning by other PostMessage
  using MessagePortFuchsia::PostMessage;

  // fuchsia::web::MessagePort implementation.
  void PostMessage(fuchsia::web::WebMessage message,
                   PostMessageCallback callback) final {
    base::WeakPtr<MessagePortFuchsia> weak_this =
        weak_factory_.GetMutableWeakPtr();
    auto status = ExtractAndHandleMessageFromFidl(std::move(message));
    if (!weak_this)
      return;

    if (status) {
      LOG(WARNING) << "Received bad message, error: "
                   << static_cast<int32_t>(*status);
      ReportPipeError();
      return;
    }

    callback(fpromise::ok());
  }

  void ReceiveMessage(ReceiveMessageCallback callback) final {
    if (pending_receive_message_callback_) {
      LOG(WARNING)
          << "ReceiveMessage called multiple times without acknowledgement.";
      ReportPipeError();
      return;
    }

    pending_receive_message_callback_ = std::move(callback);
    DeliverMessageToFidl();
  }

  fidl::Binding<fuchsia::web::MessagePort> binding_;
  ReceiveMessageCallback pending_receive_message_callback_;

  const base::WeakPtrFactory<MessagePortFuchsia> weak_factory_{this};
};
}  // namespace

// static
void MessagePortFuchsia::CreatePair(std::unique_ptr<MessagePort>* client,
                                    std::unique_ptr<MessagePort>* server) {
  fidl::InterfaceHandle<fuchsia::web::MessagePort> port0;
  fidl::InterfaceRequest<fuchsia::web::MessagePort> port1 = port0.NewRequest();
  *client = MessagePortFuchsia::Create(std::move(port0));
  *server = MessagePortFuchsia::Create(std::move(port1));
}

// static
std::unique_ptr<MessagePort> MessagePortFuchsia::Create(
    fidl::InterfaceHandle<::fuchsia::web::MessagePort> handle) {
  return std::make_unique<MessagePortFuchsiaClient>(std::move(handle));
}

// static
std::unique_ptr<MessagePort> MessagePortFuchsia::Create(
    fidl::InterfaceRequest<::fuchsia::web::MessagePort> request) {
  return std::make_unique<MessagePortFuchsiaServer>(std::move(request));
}

MessagePortFuchsia* MessagePortFuchsia::FromMessagePort(MessagePort* port) {
  CHECK(port);
  // This is safe because there is one MessagePort implementation per platform
  // and this is called internally to the implementation.
  return static_cast<MessagePortFuchsia*>(port);
}

// static
fuchsia::web::WebMessage MessagePortFuchsia::CreateWebMessage(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  fuchsia::web::WebMessage message_fidl;
  message_fidl.set_data(base::MemBufferFromString(message, message));
  if (!ports.empty()) {
    PortType expected_port_type = FromMessagePort(ports[0].get())->port_type_;
    std::vector<fuchsia::web::IncomingTransferable> incoming_transferables;
    std::vector<fuchsia::web::OutgoingTransferable> receiver_transferables;
    for (auto& port : ports) {
      MessagePortFuchsia* port_fuchsia = FromMessagePort(port.get());
      PortType port_type = port_fuchsia->port_type_;

      CHECK_EQ(expected_port_type, port_type)
          << "Only one implementation of MessagePortFuchsia can be transmitted "
             "in the same message.";
      if (expected_port_type != port_type) {
        continue;
      }

      switch (port_type) {
        case PortType::HANDLE: {
          fuchsia::web::IncomingTransferable in;
          in.set_message_port(
              reinterpret_cast<MessagePortFuchsiaClient*>(port_fuchsia)
                  ->TakeClientHandle());
          incoming_transferables.emplace_back(std::move(in));
          break;
        }
        case PortType::REQUEST: {
          fuchsia::web::OutgoingTransferable out;
          out.set_message_port(
              reinterpret_cast<MessagePortFuchsiaServer*>(port_fuchsia)
                  ->TakeServiceRequest());
          receiver_transferables.emplace_back(std::move(out));
          break;
        }
      }
    }

    message_fidl.set_incoming_transfer(std::move(incoming_transferables));
    message_fidl.set_outgoing_transfer(std::move(receiver_transferables));
  }

  return message_fidl;
}

MessagePortFuchsia::MessagePortFuchsia(PortType port_type)
    : receiver_(nullptr), port_type_(port_type) {}
MessagePortFuchsia::~MessagePortFuchsia() = default;

std::optional<fuchsia::web::FrameError>
MessagePortFuchsia::ExtractAndHandleMessageFromFidl(
    fuchsia::web::WebMessage message) {
  CHECK(receiver_);
  if (!message.has_data()) {
    return fuchsia::web::FrameError::NO_DATA_IN_MESSAGE;
  }

  std::optional<std::string> data = base::StringFromMemBuffer(message.data());
  if (!data) {
    return fuchsia::web::FrameError::BUFFER_NOT_UTF8;
  }

  std::vector<std::unique_ptr<MessagePort>> ports;
  if (message.has_incoming_transfer()) {
    for (fuchsia::web::IncomingTransferable& transferable :
         *message.mutable_incoming_transfer()) {
      ports.emplace_back(Create(std::move(transferable.message_port())));
    }
  }

  if (message.mutable_outgoing_transfer()) {
    for (fuchsia::web::OutgoingTransferable& transferable :
         *message.mutable_outgoing_transfer()) {
      ports.emplace_back(Create(std::move(transferable.message_port())));
    }
  }

  if (!receiver_->OnMessage(std::move(*data), std::move(ports))) {
    return fuchsia::web::FrameError::INTERNAL_ERROR;
  }

  return std::nullopt;
}

void MessagePortFuchsia::OnZxError(zx_status_t status) {
  ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED,
            status)
      << " MessagePort disconnected.";
  ReportPipeError();
}

void MessagePortFuchsia::ReportPipeError() {
  CHECK(receiver_);
  receiver_->OnPipeError();
}

// cast_api_bindings::MessagePortFuchsia implementation
bool MessagePortFuchsia::PostMessage(std::string_view message) {
  return PostMessageWithTransferables(message, {});
}

bool MessagePortFuchsia::PostMessageWithTransferables(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  CHECK(receiver_);
  message_queue_.emplace_back(CreateWebMessage(message, std::move(ports)));

  // Start draining the queue if it was empty beforehand.
  if (message_queue_.size() == 1u) {
    DeliverMessageToFidl();
  }

  return true;
}

}  // namespace cast_api_bindings
