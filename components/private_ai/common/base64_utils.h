// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_COMMON_BASE64_UTILS_H_
#define COMPONENTS_PRIVATE_AI_COMMON_BASE64_UTILS_H_

#include <optional>
#include <string>

#include "base/base64url.h"

namespace private_ai {

// Decodes a standard Base64 string and re-encodes it to Base64Url format with
// the specified padding policy.
// Returns std::nullopt if the input string is not a valid Base64 string.
std::optional<std::string> ConvertBase64toBase64Url(
    const std::string& input,
    base::Base64UrlEncodePolicy encode_policy);

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_COMMON_BASE64_UTILS_H_
