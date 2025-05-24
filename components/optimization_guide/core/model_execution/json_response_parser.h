// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_JSON_RESPONSE_PARSER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_JSON_RESPONSE_PARSER_H_

#include <string>
#include <string_view>

#include "components/optimization_guide/core/model_execution/response_parser.h"

namespace optimization_guide {

// A ResponseParser that just puts all of the output in a single field.
class JsonResponseParser final : public ResponseParser {
 public:
  explicit JsonResponseParser(std::string_view proto_type);

  // Parses redacted model output.
  void ParseAsync(const std::string& redacted_output,
                  ResultCallback result_callback) const override;

  bool SuppressParsingIncompleteResponse() const override;

 private:
  const std::string proto_type_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_JSON_RESPONSE_PARSER_H_
