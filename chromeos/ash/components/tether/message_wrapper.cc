// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/message_wrapper.h"

#include <memory>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace ash {

namespace tether {

namespace {

const char kJsonTypeKey[] = "type";
const char kJsonDataKey[] = "data";

std::unique_ptr<google::protobuf::MessageLite> DecodedMessageToProto(
    const MessageType& type,
    const std::string& decoded_message) {
  switch (type) {
    case MessageType::CONNECT_TETHERING_REQUEST: {
      std::unique_ptr<ConnectTetheringRequest> connect_request =
          std::make_unique<ConnectTetheringRequest>();
      connect_request->ParseFromString(decoded_message);
      return connect_request;
    }
    case MessageType::CONNECT_TETHERING_RESPONSE: {
      std::unique_ptr<ConnectTetheringResponse> connect_response =
          std::make_unique<ConnectTetheringResponse>();
      connect_response->ParseFromString(decoded_message);
      return connect_response;
    }
    case MessageType::DISCONNECT_TETHERING_REQUEST: {
      std::unique_ptr<DisconnectTetheringRequest> disconnect_request =
          std::make_unique<DisconnectTetheringRequest>();
      disconnect_request->ParseFromString(decoded_message);
      return disconnect_request;
    }
    case MessageType::KEEP_ALIVE_TICKLE: {
      std::unique_ptr<KeepAliveTickle> keep_alive_tickle =
          std::make_unique<KeepAliveTickle>();
      keep_alive_tickle->ParseFromString(decoded_message);
      return keep_alive_tickle;
    }
    case MessageType::KEEP_ALIVE_TICKLE_RESPONSE: {
      std::unique_ptr<KeepAliveTickleResponse> keep_alive_tickle_response =
          std::make_unique<KeepAliveTickleResponse>();
      keep_alive_tickle_response->ParseFromString(decoded_message);
      return keep_alive_tickle_response;
    }
    case MessageType::TETHER_AVAILABILITY_REQUEST: {
      std::unique_ptr<TetherAvailabilityRequest> tether_request =
          std::make_unique<TetherAvailabilityRequest>();
      tether_request->ParseFromString(decoded_message);
      return tether_request;
    }
    case MessageType::TETHER_AVAILABILITY_RESPONSE: {
      std::unique_ptr<TetherAvailabilityResponse> tether_response =
          std::make_unique<TetherAvailabilityResponse>();
      tether_response->ParseFromString(decoded_message);
      return tether_response;
    }
    default:
      return nullptr;
  }
}

}  // namespace

// static
std::unique_ptr<MessageWrapper> MessageWrapper::FromRawMessage(
    const std::string& message) {
  std::optional<base::Value> json_value = base::JSONReader::Read(message);
  if (!json_value) {
    return nullptr;
  }

  const base::Value::Dict* json_dictionary = json_value->GetIfDict();
  if (!json_dictionary) {
    return nullptr;
  }

  std::optional<int> message_type = json_dictionary->FindInt(kJsonTypeKey);
  if (!message_type)
    return nullptr;

  const std::string* encoded_message =
      json_dictionary->FindString(kJsonDataKey);
  if (!encoded_message)
    return nullptr;

  std::string decoded_message;
  if (!base::Base64UrlDecode(*encoded_message,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_message)) {
    return nullptr;
  }

  std::unique_ptr<google::protobuf::MessageLite> proto = DecodedMessageToProto(
      static_cast<MessageType>(*message_type), decoded_message);
  if (!proto) {
    return nullptr;
  }

  return base::WrapUnique(new MessageWrapper(
      static_cast<MessageType>(*message_type), std::move(proto)));
}

MessageWrapper::MessageWrapper(const ConnectTetheringRequest& request)
    : type_(MessageType::CONNECT_TETHERING_REQUEST),
      proto_(std::make_unique<ConnectTetheringRequest>(request)) {}

MessageWrapper::MessageWrapper(const ConnectTetheringResponse& response)
    : type_(MessageType::CONNECT_TETHERING_RESPONSE),
      proto_(std::make_unique<ConnectTetheringResponse>(response)) {}

MessageWrapper::MessageWrapper(const DisconnectTetheringRequest& request)
    : type_(MessageType::DISCONNECT_TETHERING_REQUEST),
      proto_(std::make_unique<DisconnectTetheringRequest>(request)) {}

MessageWrapper::MessageWrapper(const KeepAliveTickle& tickle)
    : type_(MessageType::KEEP_ALIVE_TICKLE),
      proto_(std::make_unique<KeepAliveTickle>(tickle)) {}

MessageWrapper::MessageWrapper(const KeepAliveTickleResponse& response)
    : type_(MessageType::KEEP_ALIVE_TICKLE_RESPONSE),
      proto_(std::make_unique<KeepAliveTickleResponse>(response)) {}

MessageWrapper::MessageWrapper(const TetherAvailabilityRequest& request)
    : type_(MessageType::TETHER_AVAILABILITY_REQUEST),
      proto_(std::make_unique<TetherAvailabilityRequest>(request)) {}

MessageWrapper::MessageWrapper(const TetherAvailabilityResponse& response)
    : type_(MessageType::TETHER_AVAILABILITY_RESPONSE),
      proto_(std::make_unique<TetherAvailabilityResponse>(response)) {}

MessageWrapper::MessageWrapper(
    const MessageType& type,
    std::unique_ptr<google::protobuf::MessageLite> proto)
    : type_(type), proto_(std::move(proto)) {}

MessageWrapper::~MessageWrapper() = default;

google::protobuf::MessageLite* MessageWrapper::GetProto() const {
  return proto_.get();
}

MessageType MessageWrapper::GetMessageType() const {
  return type_;
}

std::string MessageWrapper::ToRawMessage() const {
  std::string encoded_message;
  base::Base64UrlEncode(proto_->SerializeAsString(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_message);

  base::Value::Dict json_dictionary;
  json_dictionary.Set(kJsonTypeKey, static_cast<int>(type_));
  json_dictionary.Set(kJsonDataKey, encoded_message);

  std::string raw_message;
  base::JSONWriter::Write(json_dictionary, &raw_message);
  return raw_message;
}

}  // namespace tether

}  // namespace ash
