// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"

#include "base/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/cast_core/runtime/browser/message_port_handler.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "components/cast/message_port/blink_message_port_adapter.h"
#include "components/cast/message_port/platform_message_port.h"

namespace chromecast {

BindingsManagerWebRuntime::Client::~Client() = default;

BindingsManagerWebRuntime::BindingsManagerWebRuntime(
    Client& client,
    std::unique_ptr<MessagePortService> message_port_service)
    : message_port_service_(std::move(message_port_service)), client_(client) {}

BindingsManagerWebRuntime::~BindingsManagerWebRuntime() = default;

void BindingsManagerWebRuntime::AddBinding(base::StringPiece binding_script) {
  int id = next_script_id_++;
  bindings_[base::NumberToString(id)] = std::string(binding_script);
}

cast_receiver::Status BindingsManagerWebRuntime::HandleMessage(
    cast::web::Message message) {
  return message_port_service_->HandleMessage(std::move(message));
}

void BindingsManagerWebRuntime::ConfigureWebContents(
    content::WebContents* web_contents) {
  DCHECK(!message_port_connector_);

  message_port_connector_ =
      std::make_unique<cast_receiver::BindingsMessagePortConnector>(
          web_contents, *this);
  message_port_connector_->ConnectToBindingsService();
}

void BindingsManagerWebRuntime::OnError() {
  message_port_connector_.reset();
  client_->OnError();
}

void BindingsManagerWebRuntime::AddBinding(base::StringPiece binding_name,
                                           base::StringPiece binding_script) {
  bindings_[std::string(binding_name)] = std::string(binding_script);
}

std::vector<cast_receiver::BindingsMessagePortConnector::Client::ApiBinding>
BindingsManagerWebRuntime::GetAllBindings() {
  std::vector<ApiBinding> bindings_vector;
  for (auto& [name, script] : bindings_) {
    ApiBinding api_binding{script};
    bindings_vector.push_back(std::move(api_binding));
  }
  return bindings_vector;
}

void BindingsManagerWebRuntime::Connect(const std::string& port_name,
                                        blink::MessagePortDescriptor port) {
  message_port_service_->ConnectToPortAsync(
      port_name,
      cast_api_bindings::BlinkMessagePortAdapter::ToClientPlatformMessagePort(
          blink::WebMessagePort::Create(std::move(port))));
}

}  // namespace chromecast
