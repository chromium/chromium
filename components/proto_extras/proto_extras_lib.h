// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROTO_EXTRAS_PROTO_EXTRAS_LIB_H_
#define COMPONENTS_PROTO_EXTRAS_PROTO_EXTRAS_LIB_H_

#include <string>
#include <type_traits>

#include "base/component_export.h"
#include "base/base64.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "third_party/protobuf/src/google/protobuf/message.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace google::protobuf {
class UnknownFieldSet;
}  // namespace google::protobuf

namespace proto_extras {

COMPONENT_EXPORT(PROTO_EXTRAS) base::DictValue Serialize(
    const google::protobuf::UnknownFieldSet& unknown_fields);

// Specialization for Message protos, which use the UnknownFieldSet type
// allowing for more readable serialization.
template <typename MessageType,
          typename std::enable_if_t<
              std::is_base_of<google::protobuf::Message, MessageType>::value,
              int> = 0>
void SerializeUnknownFields(const MessageType& message, base::DictValue& dict) {
  if (message.unknown_fields().empty()) {
    return;
  }
  dict.Set("unknown_fields", Serialize(message.unknown_fields()));
}

// Specialization for MessageLite protos. These types don't use the
// UnknownFieldSet, and instead store all unknown fields in a string of bytes.
template <
    typename MessageType,
    typename std::enable_if<
        std::is_base_of<google::protobuf::MessageLite, MessageType>::value &&
            !std::is_base_of<google::protobuf::Message, MessageType>::value,
        int>::type = 0>
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

}  // namespace proto_extras

#endif  // COMPONENTS_PROTO_EXTRAS_PROTO_EXTRAS_LIB_H_
