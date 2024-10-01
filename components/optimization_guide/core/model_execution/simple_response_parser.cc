// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/simple_response_parser.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"

namespace optimization_guide {

SimpleResponseParser::SimpleResponseParser(
    const proto::OnDeviceModelExecutionOutputConfig& config)
    : config_(config) {}
SimpleResponseParser::~SimpleResponseParser() = default;

void SimpleResponseParser::ParseAsync(const std::string& redacted_output,
                                      ResultCallback result_callback) const {
  auto result = SetProtoValue(config_.proto_type(), config_.proto_field(),
                              redacted_output);
  if (!result) {
    std::move(result_callback)
        .Run(base::unexpected(ResponseParsingError::kFailed));
    return;
  }
  std::move(result_callback).Run(*result);
}

bool SimpleResponseParser::SuppressParsingIncompleteResponse() const {
  return config_.suppress_parsing_incomplete_output();
}

SimpleResponseParserFactory::SimpleResponseParserFactory() = default;
SimpleResponseParserFactory::~SimpleResponseParserFactory() = default;

std::unique_ptr<ResponseParser> SimpleResponseParserFactory::CreateParser(
    const proto::OnDeviceModelExecutionOutputConfig& config) {
  return std::make_unique<SimpleResponseParser>(config);
}

}  // namespace optimization_guide
