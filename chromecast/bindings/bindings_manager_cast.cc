// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_cast.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/bindings/grit/resources.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace bindings {

namespace {

const char kNamedMessagePortConnectorBindingsId[] =
    "NAMED_MESSAGE_PORT_CONNECTOR";
const char kControlPortConnectMessage[] = "cast.master.connect";

}  // namespace

BindingsManagerCast::BindingsManagerCast() : cast_web_contents_(nullptr) {
  // NamedMessagePortConnector binding will be injected into page first.
  AddBinding(kNamedMessagePortConnectorBindingsId,
             ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                 IDR_PORT_CONNECTOR_JS));
}

BindingsManagerCast::~BindingsManagerCast() = default;

void BindingsManagerCast::AddBinding(base::StringPiece binding_name,
                                     base::StringPiece binding_script) {
  bindings_by_id_[binding_name.as_string()] = binding_script.as_string();
}

void BindingsManagerCast::AttachToPage(
    chromecast::CastWebContents* cast_web_contents) {
  DCHECK(!cast_web_contents_) << "AttachToPage() was called twice.";
  DCHECK(cast_web_contents);

  cast_web_contents_ = cast_web_contents;
  CastWebContents::Observer::Observe(cast_web_contents_);

  for (const auto& binding : bindings_by_id_) {
    LOG(INFO) << "Register bindings for page. bindingId: " << binding.first;
    cast_web_contents_->AddBeforeLoadJavaScript(
        binding.first /* binding ID */, {"*"}, binding.second /* binding JS */);
  }
}

void BindingsManagerCast::OnPageStateChanged(
    CastWebContents* cast_web_contents) {
  auto page_state = cast_web_contents->page_state();

  switch (page_state) {
    case CastWebContents::PageState::IDLE:
    case CastWebContents::PageState::LOADING:
    case CastWebContents::PageState::CLOSED:
      return;
    case CastWebContents::PageState::DESTROYED:
    case CastWebContents::PageState::ERROR:
      connector_.reset();
      CastWebContents::Observer::Observe(nullptr);
      cast_web_contents_ = nullptr;
      return;
    case CastWebContents::PageState::LOADED:
      OnPageLoaded();
      return;
  }
}

void BindingsManagerCast::OnPageLoaded() {
  DCHECK(cast_web_contents_)
      << "Received PageLoaded event while not observing a page";

  // Unbind platform-side MessagePort connector.
  if (connector_) {
    connector_->set_incoming_receiver(nullptr);
    connector_.reset();
  }

  // Create a pre-connected MessagePipe, this is the way chromium
  // implements HTML5 MessagePort.
  mojo::ScopedMessagePipeHandle platform_port;
  mojo::ScopedMessagePipeHandle page_port;
  mojo::CreateMessagePipe(nullptr, &platform_port, &page_port);

  connector_ = std::make_unique<mojo::Connector>(
      std::move(platform_port), mojo::Connector::SINGLE_THREADED_SEND,
      base::ThreadTaskRunnerHandle::Get());
  connector_->set_connection_error_handler(base::BindOnce(
      &BindingsManagerCast::OnControlPortDisconnected, base::Unretained(this)));
  connector_->set_incoming_receiver(this);

  // Post page_port to the page so that we could receive messages
  // through another end of the pipe, which is platform_port.
  // |named_message_port_connector.js| will receive this through
  // onmessage event.
  std::vector<mojo::ScopedMessagePipeHandle> message_ports;
  message_ports.push_back(std::move(page_port));
  cast_web_contents_->PostMessageToMainFrame("*", kControlPortConnectMessage,
                                             std::move(message_ports));
}

bool BindingsManagerCast::Accept(mojo::Message* message) {
  // Receive MessagePort and forward ports to their corresponding
  // binding handlers.
  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(*message), &transferable_message)) {
    return false;
  }

  // One and only one MessagePort should be sent to here.
  if (transferable_message.ports.empty()) {
    LOG(ERROR) << "TransferableMessage contains no ports.";
  }
  DCHECK(transferable_message.ports.size() == 1)
      << "Only one control port should be provided";
  blink::MessagePortChannel message_port_channel =
      std::move(transferable_message.ports[0]);

  base::string16 data_utf16;
  if (!blink::DecodeStringMessage(transferable_message.encoded_message,
                                  &data_utf16)) {
    LOG(ERROR) << "This Message does not contain bindingId";
    return false;
  }

  std::string binding_id;
  if (!base::UTF16ToUTF8(data_utf16.data(), data_utf16.size(), &binding_id)) {
    return false;
  }

  // Route the port to corresponding binding backend.
  OnPortConnected(binding_id, message_port_channel.ReleaseHandle());
  return true;
}

void BindingsManagerCast::OnControlPortDisconnected() {
  LOG(INFO) << "NamedMessagePortConnector control port disconnected";
  connector_.reset();
}

}  // namespace bindings
}  // namespace chromecast
