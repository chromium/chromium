// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CERTIFICATE_UTIL_H_
#define COMPONENTS_PRIVATE_AI_CERTIFICATE_UTIL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace private_ai {

// Converts a PEM-formatted X.509 certificate string into raw DER bytes.
// If `pem_content` contains multiple certificate blocks, only the first
// certificate is decoded and returned.
// Returns std::nullopt if parsing fails or input is invalid.
std::optional<std::string> PemToDer(std::string_view pem_content);

// Converts a PEM-formatted X.509 certificate chain into raw DER bytes, one
// entry per CERTIFICATE block, preserving the order in which the blocks
// appear in `pem_content` (conventionally leaf first).
// Returns an empty vector if `pem_content` contains no valid certificate
// block. Blocks that fail to decode are skipped.
std::vector<std::string> PemToDerChain(std::string_view pem_content);

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CERTIFICATE_UTIL_H_
