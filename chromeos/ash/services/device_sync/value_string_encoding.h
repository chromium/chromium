// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_VALUE_STRING_ENCODING_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_VALUE_STRING_ENCODING_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ash {

namespace device_sync {

namespace util {

// NOTE: Do not change the encoding scheme because some output values are
// persisted as preferences.

// Converts input string to Base64Url-encoded std::string.
std::string EncodeAsString(const std::string& unencoded_string);

// Inverse operation to EncodeAsString(). Returns null if |encoded_string|
// cannot be decoded.
std::optional<std::string> DecodeFromString(const std::string& encoded_string);

// Converts input string to Base64Url-encoded base::Value string. This is
// particularly useful when storing byte strings as preferences because
// base::Value strings must be valid UTF-8 strings.
base::Value EncodeAsValueString(const std::string& unencoded_string);

// Inverse operation to EncodeAsValueString(). Returns null if
// |encoded_value_string| is null or cannot be decoded.
std::optional<std::string> DecodeFromValueString(
    const base::Value* encoded_value_string);

// Serializes input proto message to Base64Url-encoded base::Value string.
// |unencoded_message| cannot be null.
base::Value EncodeProtoMessageAsValueString(
    const ::google::protobuf::MessageLite* unencoded_message);

// Inverse operation to EncodeProtoMessageAsValueString(). The template class T
// must be a child of ::google::protobuf::MessageLite. Returns null if
// |encoded_value_string| is null, cannot be decoded, or proto message T cannot
// be parsed from the decoded string.
template <class T>
std::optional<T> DecodeProtoMessageFromValueString(
    const base::Value* encoded_value_string) {
  std::optional<std::string> decoded_string =
      DecodeFromValueString(encoded_value_string);
  if (!decoded_string)
    return std::nullopt;

  T decoded_message;
  if (!decoded_message.ParseFromString(*decoded_string))
    return std::nullopt;

  return decoded_message;
}

}  // namespace util

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_VALUE_STRING_ENCODING_H_
