// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROTO_EXTRAS_PROTO_EXTRAS_LIB_H_
#define COMPONENTS_PROTO_EXTRAS_PROTO_EXTRAS_LIB_H_

#include <string>
#include <type_traits>

#include "base/base64.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace absl {
class Cord;
}  // namespace absl

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace proto_extras {

// Specialization for MessageLite protos, which always returns a std::string
// for unknown fields.
template <typename MessageType>
  requires requires(MessageType message) {
    { message.unknown_fields() } -> std::same_as<const std::string&>;
  }
void SerializeUnknownFields(const MessageType& message, base::DictValue& dict) {
  if (message.unknown_fields().empty()) {
    return;
  }
  dict.Set("unknown_fields", base::Base64Encode(message.unknown_fields()));
}

template <typename T>
concept CanFitInInt = requires(T value) { base::strict_cast<int>(value); };

// Converts types to int that safely convertible to int, as base::Value only
// supports the one 'int' type.
template <typename T>
  requires CanFitInInt<T>
int ToNumericTypeForValue(T value) {
  return base::strict_cast<int>(value);
}

// Converts numeric types that are not supported by base::Value to a string.
template <typename T>
  requires std::is_arithmetic_v<T> && (!CanFitInInt<T>)
std::string ToNumericTypeForValue(T value) {
  return base::NumberToString(value);
}

// Convert an absl::Cord of bytes into a string.
std::string Base64EncodeCord(const absl::Cord& cord);

// Serialization for fields marked `debug_redact`
template <typename T>
std::string ToValueForDebugRedact(const T& value) {
  if constexpr (std::is_same_v<T, std::string>) {
    return base::StringPrintf("<%zu-byte secret>", value.size());
  } else {
    return "<secret>";
  }
}

}  // namespace proto_extras

#endif  // COMPONENTS_PROTO_EXTRAS_PROTO_EXTRAS_LIB_H_
