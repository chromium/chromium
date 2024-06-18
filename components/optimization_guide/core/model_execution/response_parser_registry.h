// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_REGISTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_REGISTRY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/parser_kind.pb.h"

namespace optimization_guide {

// Constructs response parsers for a registered type.
class ResponseParserRegistry {
 public:
  ResponseParserRegistry();
  ~ResponseParserRegistry();

  // Get the singleton instance.
  static const ResponseParserRegistry& Get();

  // Constructs a parser for the given config.
  std::unique_ptr<ResponseParser> CreateParser(
      const proto::OnDeviceModelExecutionOutputConfig& config) const;

 private:
  std::map<proto::ParserKind, std::unique_ptr<ResponseParserFactory>>
      factories_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_RESPONSE_PARSER_REGISTRY_H_
