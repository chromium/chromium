// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/bindings_manager.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/cast/message_port/blink_message_port_adapter.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/public/message_port_service.h"

namespace cast_receiver {

BindingsManager::Client::~Client() = default;

BindingsManager::BindingsManager(Client& client,
                                 MessagePortService& message_port_service)
    : message_port_service_(message_port_service), client_(client) {}

BindingsManager::~BindingsManager() = default;

void BindingsManager::AddBinding(std::string_view binding_script) {
  int id = next_script_id_++;
  bindings_[base::NumberToString(id)] = std::string(binding_script);
}

void BindingsManager::ConfigureWebContents(content::WebContents* web_contents) {
  DCHECK(!message_port_connector_);

  message_port_connector_ =
      std::make_unique<BindingsMessagePortConnector>(web_contents, *this);
  message_port_connector_->ConnectToBindingsService();
}

void BindingsManager::OnError() {
  message_port_connector_.reset();
  client_->OnError();
}

void BindingsManager::AddBinding(std::string_view binding_name,
                                 std::string_view binding_script) {
  bindings_[std::string(binding_name)] = std::string(binding_script);
}

std::vector<cast_receiver::BindingsMessagePortConnector::Client::ApiBinding>
BindingsManager::GetAllBindings() {
  std::vector<ApiBinding> bindings_vector;
  for (auto& [name, script] : bindings_) {
    ApiBinding api_binding{script};
    bindings_vector.push_back(std::move(api_binding));
  }
  return bindings_vector;
}

void BindingsManager::Connect(const std::string& port_name,
                              blink::MessagePortDescriptor port) {
  message_port_service_->ConnectToPortAsync(
      port_name,
      cast_api_bindings::BlinkMessagePortAdapter::ToClientPlatformMessagePort(
          blink::WebMessagePort::Create(std::move(port))));
}

}  // namespace cast_receiver
