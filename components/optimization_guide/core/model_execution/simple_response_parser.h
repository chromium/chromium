// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SIMPLE_RESPONSE_PARSER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SIMPLE_RESPONSE_PARSER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"

namespace optimization_guide {

// A ResponseParser that just puts all of the output in a single field.
class SimpleResponseParser final : public ResponseParser {
 public:
  explicit SimpleResponseParser(
      const proto::OnDeviceModelExecutionOutputConfig& config);
  ~SimpleResponseParser() override;

  // Parses redacted model output, returns parsed data via result_callback.
  void ParseAsync(const std::string& redacted_output,
                  ResultCallback result_callback) const override;

  bool SuppressParsingIncompleteResponse() const override;

 private:
  proto::OnDeviceModelExecutionOutputConfig config_;
};

// Constructs SimpleResponseParsers based on a config.
class SimpleResponseParserFactory : public ResponseParserFactory {
 public:
  SimpleResponseParserFactory();
  ~SimpleResponseParserFactory() override;

  // Constructs a parser for the given config.
  std::unique_ptr<ResponseParser> CreateParser(
      const proto::OnDeviceModelExecutionOutputConfig& config) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SIMPLE_RESPONSE_PARSER_H_
