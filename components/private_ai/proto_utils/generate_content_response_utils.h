// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PROTO_UTILS_GENERATE_CONTENT_RESPONSE_UTILS_H_
#define COMPONENTS_PRIVATE_AI_PROTO_UTILS_GENERATE_CONTENT_RESPONSE_UTILS_H_

#include <optional>
#include <string>

namespace private_ai {
namespace proto {
class GenerateContentResponse;
}  // namespace proto

// Converts GenerateContentResponse proto into text.
std::optional<std::string> ConvertGenerateContentResponseToText(
    const proto::GenerateContentResponse& input_proto);

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PROTO_UTILS_GENERATE_CONTENT_RESPONSE_UTILS_H_
