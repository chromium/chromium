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

NamedMessagePortConnector::NamedMessagePortConnector() = default;

NamedMessagePortConnector::~NamedMessagePortConnector() = default;

void NamedMessagePortConnector::RegisterPortHandler(
    PortConnectedCallback handler) {
  handler_ = std::move(handler);
}

// Receives the MessagePort and forwards ports to their corresponding binding
// handlers.
bool NamedMessagePortConnector::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  if (ports.size() != 1) {
    DLOG(WARNING) << "Ignoring malformed port request: expected 1 port, got "
                  << ports.size();
    return false;
  }

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(message, base::JSON_PARSE_RFC);
  if (!dict) {
    DLOG(WARNING)
        << "Ignoring malformed port request: not a valid JSON dictionary.";
    return false;
  }

  const std::string* id = dict->FindString("portId");
  if (!id || id->empty()) {
    DLOG(WARNING)
        << "Ignoring malformed port request: missing or empty portId.";
    return false;
  }

  const std::string* token_str = dict->FindString("token");
  std::optional<base::UnguessableToken> parsed_token =
      token_str ? base::UnguessableToken::DeserializeFromString(*token_str)
                : std::nullopt;
  if (!parsed_token) {
    DLOG(WARNING)
        << "Ignoring malformed port request: missing or invalid token.";
    return false;
  }
  if (*parsed_token != generation_token_) {
    DLOG(WARNING) << "Ignoring late port request from stale generation token.";
    return false;
  }

  return handler_.Run(*id, std::move(ports[0]));
}

void NamedMessagePortConnector::OnPipeError() {}

void NamedMessagePortConnector::GetConnectMessage(
    std::string* message,
    std::unique_ptr<MessagePort>* port) {
  CreatePlatformMessagePortPair(&control_port_, port);
  generation_token_ = base::UnguessableToken::Create();

  base::DictValue payload;
  payload.Set("action", "cast.master.connect");
  payload.Set("token", generation_token_.ToString());
  base::JSONWriter::Write(payload, message);

  control_port_->SetReceiver(this);
}

}  // namespace cast_api_bindings
