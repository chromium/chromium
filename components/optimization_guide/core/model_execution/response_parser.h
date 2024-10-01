// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"

namespace optimization_guide {

// A reason why parsing a model response failed.
enum class ResponseParsingError {
  // Response did not have the expected structure, or similar parsing errors.
  kFailed = 1,
  // Response potentially contained disallowed PII.
  kRejectedPii = 2,
};

// A method for converting model responses to structured data.
class ResponseParser {
 public:
  virtual ~ResponseParser() = 0;
  using Result = base::expected<proto::Any, ResponseParsingError>;
  using ResultCallback = base::OnceCallback<void(Result)>;

  // Parses redacted model output, returns parsed data via result_callback.
  virtual void ParseAsync(const std::string& redacted_output,
                          ResultCallback result_callback) const = 0;

  virtual bool SuppressParsingIncompleteResponse() const = 0;

 protected:
};

// Constructs response parsers for a registered type.
class ResponseParserFactory {
 public:
  virtual ~ResponseParserFactory() = 0;

  // Constructs a parser for the given config.
  virtual std::unique_ptr<ResponseParser> CreateParser(
      const proto::OnDeviceModelExecutionOutputConfig& config) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_H_
