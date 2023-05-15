// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_PROTO_STRING_BYTES_CONVERSION_H_
#define COMPONENTS_TRUSTED_VAULT_PROTO_STRING_BYTES_CONVERSION_H_

#include <string>
#include <vector>

#include "base/containers/span.h"

namespace trusted_vault {

// Helper function for filling protobuf bytes field: protobuf represent them as
// std::string, while in code std::vector<uint8_t> or base::span<uint8_t> is
// more common.
void AssignBytesToProtoString(base::span<const uint8_t> bytes,
                              std::string* bytes_proto_field);

// Helper function for converting protobuf bytes fields (that are represented as
// std::string) to std::vector<uint8_t>, that is more convenient representation
// of bytes in the code.
std::vector<uint8_t> ProtoStringToBytes(const std::string& bytes_string);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_PROTO_STRING_BYTES_CONVERSION_H_
