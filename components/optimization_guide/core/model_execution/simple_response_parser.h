// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SIMPLE_RESPONSE_PARSER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SIMPLE_RESPONSE_PARSER_H_

#include <string>
#include <string_view>

#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/descriptors.pb.h"

namespace optimization_guide {

// A ResponseParser that just puts all of the output in a single field.
class SimpleResponseParser final : public ResponseParser {
 public:
  SimpleResponseParser(std::string_view proto_type,
                       const proto::ProtoField& proto_field,
                       bool suppress_parsing_incomplete_response);

  // Parses redacted model output, returns parsed data via result_callback.
  void ParseAsync(const std::string& redacted_output,
                  ResultCallback result_callback) const override;

  bool SuppressParsingIncompleteResponse() const override;

 private:
  const std::string proto_type_;
  const proto::ProtoField proto_field_;
  const bool suppress_parsing_incomplete_response_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SIMPLE_RESPONSE_PARSER_H_
