// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/quick_start_message.h"

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"
#include "sandbox/policy/sandbox.h"

namespace {

constexpr char kSecondDeviceAuthPayloadKey[] = "secondDeviceAuthPayload";
constexpr char kBootstrapConfigurationsPayloadKey[] = "bootstrapConfigurations";
constexpr char kBootstrapOptionsPayloadKey[] = "bootstrapOptions";
constexpr char kQuickStartPayloadKey[] = "quickStartPayload";
constexpr char kBootstrapStateKey[] = "bootstrapState";

std::string GetStringKeyForQuickStartMessageType(
    ash::quick_start::QuickStartMessageType message_type) {
  switch (message_type) {
    case ash::quick_start::QuickStartMessageType::kSecondDeviceAuthPayload:
      return kSecondDeviceAuthPayloadKey;
    case ash::quick_start::QuickStartMessageType::kBootstrapConfigurations:
      // For BootstrapConfigurations, we use the top-level payload since
      // multiple keys need to be set.
      return std::string();
    case ash::quick_start::QuickStartMessageType::kBootstrapOptions:
      return kBootstrapOptionsPayloadKey;
    case ash::quick_start::QuickStartMessageType::kQuickStartPayload:
      return kQuickStartPayloadKey;
    case ash::quick_start::QuickStartMessageType::kBootstrapState:
      // For BootstrapState, use the top-level payload.
      return std::string();
  }
}

bool IsMessagePayloadBase64Encoded(
    ash::quick_start::QuickStartMessageType message_type) {
  switch (message_type) {
    case ash::quick_start::QuickStartMessageType::kSecondDeviceAuthPayload:
    case ash::quick_start::QuickStartMessageType::kBootstrapOptions:
    case ash::quick_start::QuickStartMessageType::kBootstrapConfigurations:
    case ash::quick_start::QuickStartMessageType::kBootstrapState:
      return false;
    case ash::quick_start::QuickStartMessageType::kQuickStartPayload:
      return true;
  }
}

}  // namespace

namespace ash::quick_start {
// static
base::expected<std::unique_ptr<QuickStartMessage>, QuickStartMessage::ReadError>
QuickStartMessage::ReadMessage(std::vector<uint8_t> data) {
  std::string str_data(data.begin(), data.end());
  std::optional<base::Value> data_value = base::JSONReader::Read(str_data);
  if (!data_value.has_value()) {
    LOG(ERROR) << "Message is not JSON";
    return base::unexpected(QuickStartMessage::ReadError::INVALID_JSON);
  }

  if (!data_value->is_dict()) {
    LOG(ERROR) << "Message is not a JSON dictionary";
    return base::unexpected(QuickStartMessage::ReadError::INVALID_JSON);
  }

  base::Value::Dict& message = data_value.value().GetDict();
  base::Value::Dict* payload;
  std::string* encoded_json_payload;

  if (message.FindDict(kBootstrapConfigurationsPayloadKey)) {
    // BootstrapConfigurations needs to have a higher precedence than
    // QuickStartPayload since a BootstrapConfigurations message may also
    // contain a QuickStartPayload.
    return std::make_unique<QuickStartMessage>(
        ash::quick_start::QuickStartMessageType::kBootstrapConfigurations,
        message.Clone());
  } else if ((encoded_json_payload =
                  message.FindString(kQuickStartPayloadKey))) {
    std::string json_payload;
    bool base64_decoding_succeeded =
        base::Base64Decode(*encoded_json_payload, &json_payload,
                           base::Base64DecodePolicy::kForgiving);
    if (!base64_decoding_succeeded) {
      LOG(ERROR) << "quickStartPayload does not contain a valid base64 encoded "
                    "payload";
      return base::unexpected(
          QuickStartMessage::ReadError::BASE64_DESERIALIZATION_FAILURE);
    }

    std::optional<base::Value> json_reader_result =
        base::JSONReader::Read(json_payload);
    if (!json_reader_result.has_value()) {
      LOG(ERROR) << "Unable to decode base64 encoded payload into JSON";
      return base::unexpected(
          QuickStartMessage::ReadError::BASE64_DESERIALIZATION_FAILURE);
    }

    payload = json_reader_result->GetIfDict();

    if (payload == nullptr) {
      LOG(ERROR) << "Payload is not a JSON dictionary";
      return base::unexpected(
          QuickStartMessage::ReadError::BASE64_DESERIALIZATION_FAILURE);
    }

    return std::make_unique<QuickStartMessage>(
        ash::quick_start::QuickStartMessageType::kQuickStartPayload,
        payload->Clone());
  } else if ((payload = message.FindDict(kSecondDeviceAuthPayloadKey))) {
    return std::make_unique<QuickStartMessage>(
        ash::quick_start::QuickStartMessageType::kSecondDeviceAuthPayload,
        payload->Clone());
  } else if ((payload = message.FindDict(kBootstrapOptionsPayloadKey))) {
    return std::make_unique<QuickStartMessage>(
        ash::quick_start::QuickStartMessageType::kBootstrapOptions,
        payload->Clone());
  } else if (message.FindInt(kBootstrapStateKey)) {
    // BootstrapState needs to have the lowest precedence since other messages
    // may also contain a BootstrapState payload. The BootstrapState payload in
    // those messages may be safely ignored.
    return std::make_unique<QuickStartMessage>(
        ash::quick_start::QuickStartMessageType::kBootstrapState,
        message.Clone());
  } else {
    LOG(ERROR) << "Message does not contain known payload.";
    return base::unexpected(
        QuickStartMessage::ReadError::MISSING_MESSAGE_PAYLOAD);
  }
}

// static
base::expected<std::unique_ptr<QuickStartMessage>, QuickStartMessage::ReadError>
QuickStartMessage::ReadMessage(std::vector<uint8_t> data,
                               QuickStartMessageType message_type) {
  auto result = ReadMessage(data);
  if (result.has_value() && result.value()->get_type() != message_type) {
    LOG(ERROR) << "Unexpected message type: received="
               << result.value()->get_type() << ", expected=" << message_type;
    return base::unexpected(
        QuickStartMessage::ReadError::UNEXPECTED_MESSAGE_TYPE);
  }
  return result;
}

QuickStartMessage::QuickStartMessage(QuickStartMessageType message_type)
    : message_type_(message_type) {
  payload_ = base::Value::Dict();
}

QuickStartMessage::QuickStartMessage(QuickStartMessageType message_type,
                                     base::Value::Dict payload)
    : message_type_(message_type), payload_(std::move(payload)) {}

QuickStartMessage::~QuickStartMessage() = default;

base::Value::Dict* QuickStartMessage::GetPayload() {
  return &payload_;
}

std::unique_ptr<base::Value::Dict> QuickStartMessage::GenerateEncodedMessage() {
  std::unique_ptr<base::Value::Dict> message =
      std::make_unique<base::Value::Dict>();
  std::string str_payload_key =
      GetStringKeyForQuickStartMessageType(message_type_);
  if (str_payload_key.empty()) {
    return std::make_unique<base::Value::Dict>(std::move(payload_));
  }

  bool base64_encoded_payload_ = IsMessagePayloadBase64Encoded(message_type_);
  if (!base64_encoded_payload_) {
    message->Set(str_payload_key, std::move(payload_));
  } else {
    std::string json;
    bool json_writer_succeeded = base::JSONWriter::Write(payload_, &json);

    if (!json_writer_succeeded) {
      LOG(ERROR) << "Failed to write message payload to JSON.";
      return nullptr;
    }

    std::string base64_payload = base::Base64Encode(json);

    message->Set(str_payload_key, base64_payload);
  }
  return message;
}

}  // namespace ash::quick_start
