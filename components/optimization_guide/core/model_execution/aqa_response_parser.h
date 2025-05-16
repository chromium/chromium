// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_AQA_RESPONSE_PARSER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_AQA_RESPONSE_PARSER_H_

#include <string_view>

#include "components/optimization_guide/core/model_execution/response_parser.h"

namespace optimization_guide {
class AqaResponseParser final : public ResponseParser {
 public:
  static bool CanParse(std::string_view proto_type);

  // Parse redacted model output, returns parsed data via result_callback.
  void ParseAsync(const std::string& model_output,
                  ResultCallback result_callback) const override;

  bool SuppressParsingIncompleteResponse() const override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_AQA_RESPONSE_PARSER_H_
