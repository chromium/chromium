// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/receiver_response.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/mirroring/service/value_util.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/writer.h"

namespace mirroring {
namespace {

// Get the response type from the type string value in the JSON message.
ResponseType ResponseTypeFromString(const std::string& type) {
  if (type == "ANSWER")
    return ResponseType::ANSWER;
  if (type == "CAPABILITIES_RESPONSE")
    return ResponseType::CAPABILITIES_RESPONSE;
  if (type == "RPC")
    return ResponseType::RPC;

  return ResponseType::UNKNOWN;
}

// JSON helper methods. Note that these are *heavily* based on the
// util/json_helpers.h methods from Open Screen. When the mirroring service
// moves to depend on libcast, the duplicate code should pretty much all go
// away.
bool GetInt(const Json::Value& value, int* out) {
  // We are generally very forgiving of missing fields, so don't return an
  // error if it is just missing.
  if (!value) {
    *out = -1;
    return true;
  }
  // If it's present, though, it must be valid.
  if (!value.isInt()) {
    return false;
  }
  const int i = value.asInt();
  if (i < 0) {
    return false;
  }
  *out = i;
  return true;
}

bool GetString(const Json::Value& value, std::string* out) {
  if (!value) {
    *out = {};
    return true;
  }
  if (!value.isString()) {
    return false;
  }
  *out = value.asString();
  return true;
}

template <typename T>
using Parser = base::RepeatingCallback<bool(const Json::Value&, T*)>;

// Returns whether or not an error occurred. For the purpose of this function,
// if the value is empty or not an array, that is not an error, and it thus
// returns true after setting the |out| vector to empty.
template <typename T>
bool GetArray(const Json::Value& value, Parser<T> parser, std::vector<T>* out) {
  out->clear();
  if (!value.isArray() || value.empty()) {
    return true;
  }

  out->reserve(value.size());
  for (auto i : value) {
    T v;
    if (!parser.Run(i, &v)) {
      out->clear();
      return false;
    }
    out->push_back(std::move(v));
  }

  return true;
}

bool GetStringArray(const Json::Value& value, std::vector<std::string>* out) {
  return GetArray<std::string>(value, base::BindRepeating(&GetString), out);
}

ResponseType GetResponseType(const Json::Value& root_node) {
  std::string type;
  if (!GetString(root_node["type"], &type)) {
    return ResponseType::UNKNOWN;
  }

  return ResponseTypeFromString(base::ToUpperASCII(type));
}

std::string GetDetails(const Json::Value& value) {
  if (!value) {
    return {};
  }

  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, value);
}

std::unique_ptr<ReceiverError> ParseError(const Json::Value& value) {
  auto error = std::make_unique<ReceiverError>();

  if (!GetInt(value["code"], &(error->code)) ||
      !GetString(value["description"], &(error->description))) {
    return {};
  }

  // We are generally pretty forgiving about details: throwing an error
  // because the Receiver didn't properly fill out the detail of an error
  // message doesn't really make sense.
  error->details = GetDetails(value["details"]);
  return error;
}

std::unique_ptr<ReceiverCapability> ParseCapability(const Json::Value& value) {
  auto capability = std::make_unique<ReceiverCapability>();

  if (!value)
    return {};

  if (!GetInt(value["remoting"], &(capability->remoting))) {
    capability->remoting = ReceiverCapability::kRemotingVersionUnknown;
  }

  if (!GetStringArray(value["mediaCaps"], &(capability->media_caps))) {
    return {};
  }

  return capability;
}

}  // namespace

ReceiverCapability::ReceiverCapability() = default;
ReceiverCapability::~ReceiverCapability() = default;
ReceiverCapability::ReceiverCapability(ReceiverCapability&& receiver_response) =
    default;
ReceiverCapability::ReceiverCapability(
    const ReceiverCapability& receiver_response) = default;
ReceiverCapability& ReceiverCapability::operator=(
    ReceiverCapability&& receiver_response) = default;
ReceiverCapability& ReceiverCapability::operator=(
    const ReceiverCapability& receiver_response) = default;

ReceiverError::ReceiverError() = default;
ReceiverError::~ReceiverError() = default;
ReceiverError::ReceiverError(ReceiverError&& receiver_response) = default;
ReceiverError::ReceiverError(const ReceiverError& receiver_response) = default;
ReceiverError& ReceiverError::operator=(ReceiverError&& receiver_response) =
    default;
ReceiverError& ReceiverError::operator=(
    const ReceiverError& receiver_response) = default;

ReceiverResponse::ReceiverResponse() = default;
ReceiverResponse::~ReceiverResponse() = default;
ReceiverResponse::ReceiverResponse(ReceiverResponse&& receiver_response) =
    default;
ReceiverResponse& ReceiverResponse::operator=(
    ReceiverResponse&& receiver_response) = default;

// static
std::unique_ptr<ReceiverResponse> ReceiverResponse::Parse(
    const std::string& message_data) {
  Json::CharReaderBuilder builder;
  Json::CharReaderBuilder::strictMode(&builder.settings_);
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

  Json::Value root_node;
  std::string error_msg;
  const bool succeeded = reader->parse(
      message_data.data(), message_data.data() + message_data.length(),
      &root_node, &error_msg);
  if (!succeeded) {
    DVLOG(1) << "Failed to parse reciever message: " << error_msg;
    return nullptr;
  }

  auto response = std::make_unique<ReceiverResponse>();
  std::string result;
  if (!root_node || !GetInt(root_node["sessionId"], &(response->session_id_)) ||
      !GetInt(root_node["seqNum"], &(response->sequence_number_)) ||
      !GetString(root_node["result"], &result)) {
    return nullptr;
  }

  response->type_ = GetResponseType(root_node);

  // For backwards compatibility with <= M85, RPC responses lack a result field.
  response->valid_ = (result == "ok" || response->type_ == ResponseType::RPC);
  if (!response->valid_) {
    response->error_ = ParseError(root_node["error"]);
    return response;
  }

  switch (response->type_) {
    case ResponseType::ANSWER:
      response->answer_ = std::make_unique<openscreen::cast::Answer>();
      if (!openscreen::cast::Answer::TryParse(root_node["answer"],
                                              response->answer_.get())) {
        response->valid_ = false;
      }
      break;

    case ResponseType::CAPABILITIES_RESPONSE:
      response->capabilities_ = ParseCapability(root_node["capabilities"]);
      if (!response->capabilities_) {
        response->valid_ = false;
      }
      break;

    case ResponseType::RPC: {
      std::string raw_rpc;
      if (!GetString(root_node["rpc"], &raw_rpc) ||
          !base::Base64Decode(raw_rpc, &(response->rpc_))) {
        response->valid_ = false;
      }
    } break;

    case ResponseType::UNKNOWN:
    default:
      response->valid_ = false;
      break;
  }

  return response;
}

std::unique_ptr<ReceiverResponse> ReceiverResponse::CloneForTesting() const {
  auto clone = std::make_unique<ReceiverResponse>();
  clone->type_ = type_;
  clone->session_id_ = session_id_;
  clone->sequence_number_ = sequence_number_;
  clone->valid_ = valid_;
  if (!valid_) {
    if (error_) {
      clone->error_ = std::make_unique<ReceiverError>(*error_);
    }
    return clone;
  }

  // We assume that if the message wasn't classified as an error,
  // it has a body.
  switch (type_) {
    case ResponseType::ANSWER:
      clone->answer_ = std::make_unique<openscreen::cast::Answer>(*answer_);
      break;
    case ResponseType::CAPABILITIES_RESPONSE:
      clone->capabilities_ =
          std::make_unique<ReceiverCapability>(*capabilities_);
      break;
    case ResponseType::RPC:
      clone->rpc_ = rpc_;
      break;
    case ResponseType::UNKNOWN:
      break;
  }
  return clone;
}

// static
ReceiverResponse ReceiverResponse::CreateAnswerResponseForTesting(
    int32_t sequence_number,
    std::unique_ptr<openscreen::cast::Answer> answer) {
  ReceiverResponse response;
  response.type_ = ResponseType::ANSWER;
  response.sequence_number_ = sequence_number;
  response.answer_ = std::move(answer);
  response.valid_ = true;
  return response;
}

// static
ReceiverResponse ReceiverResponse::CreateCapabilitiesResponseForTesting(
    int32_t sequence_number,
    std::unique_ptr<ReceiverCapability> capabilities) {
  ReceiverResponse response;
  response.type_ = ResponseType::CAPABILITIES_RESPONSE;
  response.sequence_number_ = sequence_number;
  response.capabilities_ = std::move(capabilities);
  response.valid_ = true;
  return response;
}

}  // namespace mirroring
