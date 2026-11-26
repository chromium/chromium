// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/named_message_port_connector/named_message_port_connector.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/cast/message_port/platform_message_port.h"

namespace cast_api_bindings {
namespace {

constexpr std::string_view kMasterConnectedPortId = "cast.master.connected";

std::optional<std::string> ParsePortRequestId(
    std::string_view message,
    const base::UnguessableToken& expected_token) {
  if (expected_token.is_empty()) {
    DLOG(WARNING) << "Ignoring port request with no active generation token.";
    return std::nullopt;
  }

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(message, base::JSON_PARSE_RFC);
  if (!dict) {
    DLOG(WARNING)
        << "Ignoring malformed port request: not a valid JSON dictionary.";
    return std::nullopt;
  }

  const std::string* id = dict->FindString("portId");
  if (!id || id->empty()) {
    DLOG(WARNING)
        << "Ignoring malformed port request: missing or empty portId.";
    return std::nullopt;
  }

  const std::string* token_str = dict->FindString("token");
  std::optional<base::UnguessableToken> parsed_token =
      token_str ? base::UnguessableToken::DeserializeFromString(*token_str)
                : std::nullopt;
  if (!parsed_token || *parsed_token != expected_token) {
    DLOG(WARNING)
        << "Ignoring port request with missing or stale generation token.";
    return std::nullopt;
  }

  return *id;
}

}  // namespace

NamedMessagePortConnector::NamedMessagePortConnector() = default;

NamedMessagePortConnector::~NamedMessagePortConnector() = default;

void NamedMessagePortConnector::RegisterPortHandler(
    PortConnectedCallback handler) {
  handler_ = std::move(handler);
}

void NamedMessagePortConnector::RegisterOnConnectedCallback(
    base::OnceClosure callback) {
  on_connected_callback_ = std::move(callback);
}

void NamedMessagePortConnector::Reset() {
  on_connected_callback_.Reset();
  if (control_port_) {
    control_port_->Close();
    control_port_.reset();
  }
  generation_token_ = base::UnguessableToken();
}

// Receives the MessagePort and forwards ports to their corresponding binding
// handlers.
bool NamedMessagePortConnector::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  std::optional<std::string> id =
      ParsePortRequestId(message, generation_token_);
  if (!id) {
    return false;
  }

  const bool is_master_ack = (*id == kMasterConnectedPortId);
  const size_t expected_ports = is_master_ack ? 0u : 1u;
  if (ports.size() != expected_ports) {
    DLOG(WARNING) << "Ignoring malformed port request: expected "
                  << expected_ports << " ports, got " << ports.size();
    return false;
  }

  if (on_connected_callback_) {
    std::move(on_connected_callback_).Run();
  }
  if (is_master_ack) {
    return true;
  }

  return handler_.Run(*id, std::move(ports[0]));
}

void NamedMessagePortConnector::OnPipeError() {
  Reset();
}

void NamedMessagePortConnector::GetConnectMessage(
    std::string* message,
    std::unique_ptr<MessagePort>* port) {
  Reset();
  CreatePlatformMessagePortPair(&control_port_, port);
  generation_token_ = base::UnguessableToken::Create();

  base::DictValue payload;
  payload.Set("action", "cast.master.connect");
  payload.Set("token", generation_token_.ToString());
  base::JSONWriter::Write(payload, message);

  control_port_->SetReceiver(this);
}

}  // namespace cast_api_bindings
