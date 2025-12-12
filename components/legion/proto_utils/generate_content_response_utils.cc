// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/proto_utils/generate_content_response_utils.h"

#include "components/legion/proto/legion.pb.h"

namespace legion {

std::optional<std::string> ConvertGenerateContentResponseToText(
    const proto::GenerateContentResponse& input_proto) {
  if (input_proto.candidates_size() == 0 ||
      !input_proto.candidates(0).has_content() ||
      input_proto.candidates(0).content().parts_size() == 0 ||
      !input_proto.candidates(0).content().parts(0).has_text()) {
    return std::nullopt;
  }
  return input_proto.candidates(0).content().parts(0).text();
}

}  // namespace legion
