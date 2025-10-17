// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/session_deserializer.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

namespace {
std::string SnakeToLowerCamelCase(absl::string_view input) {
  std::string result;
  if (input.empty()) {
    return result;
  }
  result.reserve(input.size());
  bool capitalize_next = false;

  for (char c : input) {
    if (c == '_') {
      capitalize_next = true;
    } else if (capitalize_next) {
      result.push_back(absl::ascii_toupper(c));
      capitalize_next = false;
    } else {
      result.push_back(c);
    }
  }
  return result;
}

const base::Value* Find(const base::DictValue& value,
                        std::string_view fieldname) {
  // ProtoJSON handles camelCase and snake_case the same. The fieldname passed
  // in should be snake_case.
  DCHECK_EQ(fieldname, base::ToLowerASCII(fieldname));

  if (auto* child = value.Find(fieldname)) {
    return child;
  }
  if (auto* child = value.Find(SnakeToLowerCamelCase(fieldname))) {
    return child;
  }
  return nullptr;
}

bool DeserializeBytes(const base::Value& value, std::string* out_string) {
  if (!value.is_string()) {
    return false;
  }
  return base::Base64Decode(value.GetString(), out_string);
}

bool DeserializeNoiseHandshakeMessage(
    const base::Value& value,
    oak::session::v1::NoiseHandshakeMessage* out_proto) {
  if (!value.is_dict()) {
    return false;
  }
  const base::Value::Dict& dict = value.GetDict();

  if (auto* ephemeral_public_key = Find(dict, "ephemeral_public_key")) {
    if (!DeserializeBytes(*ephemeral_public_key,
                          out_proto->mutable_ephemeral_public_key())) {
      return false;
    }
  }

  if (auto* static_public_key = Find(dict, "static_public_key")) {
    if (!DeserializeBytes(*static_public_key,
                          out_proto->mutable_static_public_key())) {
      return false;
    }
  }

  if (auto* ciphertext = Find(dict, "ciphertext")) {
    if (!DeserializeBytes(*ciphertext, out_proto->mutable_ciphertext())) {
      return false;
    }
  }

  return true;
}

bool DeserializeSessionBinding(const base::Value& value,
                               oak::session::v1::SessionBinding* out_proto) {
  if (!value.is_dict()) {
    return false;
  }
  const base::Value::Dict& dict = value.GetDict();

  if (auto* binding = Find(dict, "binding")) {
    if (!DeserializeBytes(*binding, out_proto->mutable_binding())) {
      return false;
    }
  }

  return true;
}

bool DeserializeSessionBindingMap(
    const base::Value& value,
    google::protobuf::Map<std::string, oak::session::v1::SessionBinding>*
        out_map) {
  if (!value.is_dict()) {
    return false;
  }

  for (auto it : value.GetDict()) {
    const std::string& key = it.first;
    const base::Value& session_binding_value = it.second;
    if (!DeserializeSessionBinding(session_binding_value, &(*out_map)[key])) {
      return false;
    }
  }
  return true;
}

bool DeserializeHandshakeResponse(
    const base::Value& value,
    oak::session::v1::HandshakeResponse* out_proto) {
  if (!value.is_dict()) {
    return false;
  }
  const base::Value::Dict& dict = value.GetDict();

  if (auto* noise_msg_value = Find(dict, "noise_handshake_message")) {
    if (!DeserializeNoiseHandshakeMessage(
            *noise_msg_value, out_proto->mutable_noise_handshake_message())) {
      return false;
    }
  }

  if (auto* attestation_bindings = Find(dict, "attestation_bindings")) {
    if (!DeserializeSessionBindingMap(
            *attestation_bindings, out_proto->mutable_attestation_bindings())) {
      return false;
    }
  }

  if (auto* assertion_bindings = Find(dict, "assertion_bindings")) {
    if (!DeserializeSessionBindingMap(
            *assertion_bindings, out_proto->mutable_assertion_bindings())) {
      return false;
    }
  }

  return true;
}
}  // namespace

bool DeserializeSessionResponse(const base::Value& value,
                                oak::session::v1::SessionResponse* out_proto) {
  if (!value.is_dict()) {
    return false;
  }
  const base::Value::Dict& dict = value.GetDict();

  if (auto* handshake_response = Find(dict, "handshake_response")) {
    if (!DeserializeHandshakeResponse(
            *handshake_response, out_proto->mutable_handshake_response())) {
      return false;
    }
  }

  if (Find(dict, "attest_response")) {
    NOTIMPLEMENTED();
  }

  if (Find(dict, "encrypted_message")) {
    NOTIMPLEMENTED();
  }

  return true;
}

}  // namespace legion
