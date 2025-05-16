// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/response_parser_factory.h"

#include "components/optimization_guide/core/model_execution/aqa_response_parser.h"
#include "components/optimization_guide/core/model_execution/json_response_parser.h"
#include "components/optimization_guide/core/model_execution/simple_response_parser.h"
#include "components/optimization_guide/proto/parser_kind.pb.h"

namespace optimization_guide {

std::unique_ptr<ResponseParser> CreateResponseParser(
    const proto::OnDeviceModelExecutionOutputConfig& output_config) {
  switch (output_config.parser_kind()) {
    case proto::PARSER_KIND_UNSPECIFIED:
    case proto::PARSER_KIND_SIMPLE:
      return std::make_unique<SimpleResponseParser>(output_config);

    case proto::PARSER_KIND_JSON:
      return std::make_unique<JsonResponseParser>(output_config);

    case proto::PARSER_KIND_AQA:
      if (!AqaResponseParser::CanParse(output_config.proto_type())) {
        return nullptr;
      }
      return std::make_unique<AqaResponseParser>(output_config);

    default:
      return nullptr;
  }
}

}  // namespace optimization_guide
