// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FIELDWISE_RESPONSE_PARSER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FIELDWISE_RESPONSE_PARSER_H_

#include <string>

#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/fieldwise_parser_config.pb.h"

namespace optimization_guide {

// A ResponseParser that parses output and saves to specific fields base on the
// configuration.
class FieldwiseResponseParser final : public ResponseParser {
 public:
  explicit FieldwiseResponseParser(std::string_view proto_type,
                                   proto::FieldwiseParserConfig& config,
                                   bool suppress_parsing_incomplete_response);

  // Parses redacted model output.
  void ParseAsync(const std::string& redacted_output,
                  ResultCallback result_callback) const override;

  bool SuppressParsingIncompleteResponse() const override;

 private:
  const std::string proto_type_;
  const proto::FieldwiseParserConfig config_;
  const bool suppress_parsing_incomplete_response_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FIELDWISE_RESPONSE_PARSER_H_
