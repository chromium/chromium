// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/proto_string_bytes_conversion.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"

namespace trusted_vault {

void AssignBytesToProtoString(base::span<const uint8_t> bytes,
                              std::string* bytes_proto_field) {
  *bytes_proto_field = std::string(bytes.begin(), bytes.end());
}

std::vector<uint8_t> ProtoStringToBytes(const std::string_view bytes_string) {
  return base::ToVector(base::as_byte_span(bytes_string));
}

}  // namespace trusted_vault
