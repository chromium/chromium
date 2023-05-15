// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/proto_string_bytes_conversion.h"

namespace trusted_vault {

void AssignBytesToProtoString(base::span<const uint8_t> bytes,
                              std::string* bytes_proto_field) {
  *bytes_proto_field = std::string(bytes.begin(), bytes.end());
}

std::vector<uint8_t> ProtoStringToBytes(const std::string& bytes_string) {
  return std::vector<uint8_t>(bytes_string.begin(), bytes_string.end());
}

}  // namespace trusted_vault
