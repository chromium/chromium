// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/json_response_parser.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace optimization_guide {

namespace {

ResponseParser::Result ExtractProto(
    std::string type_name,
    base::expected<base::Value, std::string> parse_result) {
  if (!parse_result.has_value()) {
    return base::unexpected(ResponseParsingError::kFailed);
  }
  auto result = ConvertToAnyWrappedProto(*parse_result, type_name);
  if (!result) {
    return base::unexpected(ResponseParsingError::kFailed);
  }
  return *result;
}

}  // namespace

JsonResponseParser::JsonResponseParser(
    const proto::OnDeviceModelExecutionOutputConfig& config)
    : proto_type_(config.proto_type()), config_(config) {}
JsonResponseParser::~JsonResponseParser() = default;

void JsonResponseParser::ParseAsync(const std::string& redacted_output,
                                    ResultCallback callback) const {
  data_decoder::DataDecoder::ParseJsonIsolated(
      redacted_output,
      base::BindOnce(&ExtractProto, proto_type_).Then(std::move(callback)));
}

bool JsonResponseParser::SuppressParsingIncompleteResponse() const {
  // Json parser can only parse complete responses.
  return true;
}

JsonResponseParserFactory::JsonResponseParserFactory() = default;
JsonResponseParserFactory::~JsonResponseParserFactory() = default;

std::unique_ptr<ResponseParser> JsonResponseParserFactory::CreateParser(
    const proto::OnDeviceModelExecutionOutputConfig& config) {
  return std::make_unique<JsonResponseParser>(config);
}

}  // namespace optimization_guide
