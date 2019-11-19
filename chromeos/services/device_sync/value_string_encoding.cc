// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/value_string_encoding.h"

#include "base/base64url.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace device_sync {

namespace util {

std::string EncodeAsString(const std::string& unencoded_string) {
  std::string encoded_string;
  base::Base64UrlEncode(unencoded_string,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);

  return encoded_string;
}

base::Optional<std::string> DecodeFromString(
    const std::string& encoded_string) {
  std::string decoded_string;
  if (!base::Base64UrlDecode(encoded_string,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_string)) {
    return base::nullopt;
  }

  return decoded_string;
}

base::Value EncodeAsValueString(const std::string& unencoded_string) {
  return base::Value(EncodeAsString(unencoded_string));
}

base::Optional<std::string> DecodeFromValueString(
    const base::Value* encoded_value_string) {
  if (!encoded_value_string || !encoded_value_string->is_string())
    return base::nullopt;

  return DecodeFromString(encoded_value_string->GetString());
}

base::Value EncodeProtoMessageAsValueString(
    const ::google::protobuf::MessageLite* unencoded_message) {
  DCHECK(unencoded_message);

  return EncodeAsValueString(unencoded_message->SerializeAsString());
}

}  // namespace util

}  // namespace device_sync

}  // namespace chromeos
